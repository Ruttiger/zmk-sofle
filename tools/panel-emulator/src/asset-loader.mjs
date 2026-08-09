/**
 * asset-loader.mjs
 *
 * Reads assets.json, resolves source paths, parses each asset, validates byte counts,
 * and returns a Map of loaded AssetEntry objects.
 *
 * Never throws — all errors are stored per-asset or in the top-level errors array.
 *
 * @module asset-loader
 */

import { readFile } from 'node:fs/promises';
import { resolve, dirname, isAbsolute } from 'node:path';
import { fileURLToPath } from 'node:url';

import { parseSymbol } from './asset-parser.mjs';
import { expectedByteCount } from './bitmap-renderer.mjs';

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/**
 * @typedef {Object} AssetEntry
 * @property {string}       name
 * @property {string}       source          - Resolved absolute path
 * @property {string}       symbol
 * @property {number}       width
 * @property {number}       height
 * @property {number}       frames
 * @property {string}       format
 * @property {number|null}  stride          - bytes per row if explicitly declared
 * @property {number|null}  frameStride     - bytes per frame if explicitly declared
 * @property {number|null}  offset          - byte offset into raw data
 * @property {string|null}  notes
 * @property {Uint8Array|null} bytes        - Pixel bytes (palette stripped for lvgl)
 * @property {Uint8Array|null} palette      - Palette bytes for lvgl-indexed-1bit, else null
 * @property {number}       rawByteCount    - Bytes in the C array (including any palette)
 * @property {number|null}  inferredWidth
 * @property {number|null}  inferredHeight
 * @property {boolean}      loaded          - true if file was read and symbol found
 * @property {string[]}     warnings
 * @property {string[]}     errors
 */

/**
 * @typedef {Object} LoadResult
 * @property {Map<string, AssetEntry>} assets
 * @property {Object} screen   - { width, height } from assets.json
 * @property {string[]} errors  - Top-level errors (e.g. missing assets.json)
 * @property {string[]} warnings
 */

/**
 * Load all assets declared in the given assets.json file.
 *
 * @param {string} assetsJsonPath - Absolute path to assets.json
 * @returns {Promise<LoadResult>}
 */
export async function loadAssets(assetsJsonPath) {
  /** @type {LoadResult} */
  const result = {
    assets: new Map(),
    screen: { width: 128, height: 64 },
    errors: [],
    warnings: [],
  };

  // --- Read and parse assets.json ---
  let manifest;
  try {
    const raw = await readFile(assetsJsonPath, 'utf-8');
    // Strip JS-style comments before parsing (assets.json may have _comment fields)
    manifest = JSON.parse(raw);
  } catch (err) {
    result.errors.push(`Cannot read assets.json at "${assetsJsonPath}": ${err.message}`);
    return result;
  }

  if (manifest.screen) {
    result.screen = {
      width:  manifest.screen.width  ?? 128,
      height: manifest.screen.height ?? 64,
    };
  }

  if (!Array.isArray(manifest.assets)) {
    result.warnings.push('assets.json has no "assets" array; nothing to load.');
    return result;
  }

  const manifestDir = dirname(assetsJsonPath);

  // --- Load each asset ---
  for (const decl of manifest.assets) {
    if (typeof decl.name !== 'string' || !decl.name) {
      result.warnings.push('Skipping asset declaration with missing "name" field.');
      continue;
    }

    const entry = await loadSingleAsset(decl, manifestDir);
    result.assets.set(entry.name, entry);
  }

  return result;
}

// ---------------------------------------------------------------------------
// Internals
// ---------------------------------------------------------------------------

/**
 * Load, parse and validate a single asset declaration.
 *
 * @param {Object} decl        - Raw declaration from assets.json
 * @param {string} manifestDir - Directory of assets.json (for relative path resolution)
 * @returns {Promise<AssetEntry>}
 */
async function loadSingleAsset(decl, manifestDir) {
  /** @type {AssetEntry} */
  const entry = {
    name:           decl.name,
    source:         '',
    symbol:         decl.symbol ?? decl.name,
    width:          decl.width  ?? 0,
    height:         decl.height ?? 0,
    frames:         decl.frames ?? 1,
    format:         decl.format ?? 'lvgl-indexed-1bit',
    stride:         decl.stride        ?? null,
    frameStride:    decl.frameStride   ?? null,
    offset:         decl.offset        ?? null,
    notes:          decl.notes         ?? null,
    bytes:          null,
    palette:        null,
    rawByteCount:   0,
    inferredWidth:  null,
    inferredHeight: null,
    loaded:         false,
    warnings:       [],
    errors:         [],
  };

  // --- Resolve source path ---
  if (!decl.source) {
    entry.errors.push(`Asset "${decl.name}": missing "source" field in assets.json.`);
    return entry;
  }
  entry.source = isAbsolute(decl.source)
    ? decl.source
    : resolve(manifestDir, decl.source);

  // --- Read source file ---
  let sourceText;
  try {
    sourceText = await readFile(entry.source, 'utf-8');
  } catch (err) {
    entry.errors.push(`Asset "${decl.name}": cannot read file "${entry.source}": ${err.message}`);
    return entry;
  }

  // --- Parse ---
  const parsed = parseSymbol(sourceText, entry.symbol, entry.format);
  entry.warnings.push(...parsed.warnings);
  entry.errors.push(...parsed.errors);

  if (parsed.errors.length > 0) return entry;

  entry.bytes          = parsed.bytes;
  entry.palette        = parsed.palette;
  entry.rawByteCount   = parsed.rawByteCount;
  entry.inferredWidth  = parsed.inferredWidth;
  entry.inferredHeight = parsed.inferredHeight;
  entry.loaded         = true;

  // --- Validate byte count ---
  if (entry.width > 0 && entry.height > 0) {
    validateByteCount(entry);
  } else if (entry.inferredWidth !== null && entry.inferredHeight !== null) {
    // Patch missing dimensions from inferred values and validate
    if (entry.width  === 0) { entry.width  = entry.inferredWidth;  }
    if (entry.height === 0) { entry.height = entry.inferredHeight; }
    validateByteCount(entry);
  } else {
    entry.warnings.push(
      `Asset "${entry.name}": width/height not declared in assets.json and could not be ` +
      `inferred from source. Cannot validate byte count.`
    );
  }

  return entry;
}

/**
 * Compare actual byte count against what the declared format/dimensions require.
 * Appends warnings to entry if they don't match.
 *
 * @param {AssetEntry} entry
 */
function validateByteCount(entry) {
  if (entry.width <= 0 || entry.height <= 0) return;

  const expected = entry.frameStride !== null
    ? entry.frameStride * entry.frames
    : expectedByteCount(entry.width, entry.height, entry.frames, entry.format);

  const actual = entry.bytes ? entry.bytes.length : 0;

  if (actual === expected) return; // all good

  const diff = actual - expected;
  entry.warnings.push(
    `Asset "${entry.name}": declared ${entry.width}×${entry.height} ${entry.frames} frame(s) ` +
    `format "${entry.format}" expects ${expected} pixel bytes, but got ${actual} ` +
    `(${diff > 0 ? '+' : ''}${diff}). ` +
    `Check width, height, frames, format, stride or frameStride in assets.json.`
  );
}

// ---------------------------------------------------------------------------
// Utility: reload helper (for POST /api/reload-assets in server)
// ---------------------------------------------------------------------------

/**
 * Return a fresh LoadResult by re-reading assets.json and all source files.
 * Identical to loadAssets but named more clearly for call sites.
 *
 * @param {string} assetsJsonPath
 * @returns {Promise<LoadResult>}
 */
export const reloadAssets = loadAssets;
