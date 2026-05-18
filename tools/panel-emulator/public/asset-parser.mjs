/**
 * asset-parser.mjs
 *
 * Parses C source files and extracts raw byte arrays used by ZMK/LVGL firmware.
 *
 * Supported source formats:
 *   - LVGL LV_IMG_CF_INDEXED_1BIT  (8-byte palette prefix + 1bpp pixel data)
 *   - Generic uint8_t symbol[] = { ... }
 *   - Generic unsigned char symbol[] = { ... }
 *   - Hex literals: 0xff, 0x3C
 *   - Binary literals: 0b00011000
 *   - Decimal literals: 255, 128
 *
 * Returns a ParseResult object; never throws — errors are returned as strings in `.errors[]`.
 *
 * @module asset-parser
 */

// ---------------------------------------------------------------------------
// Regex helpers
// ---------------------------------------------------------------------------

/** Matches the opening of a C array declaration we care about. */
const RE_ARRAY_DECL = /(?:static\s+)?(?:const\s+)?(?:uint8_t|unsigned\s+char)\s+(\w+)\s*\[\s*\d*\s*\]\s*=/;

/** Matches an LVGL lv_img_dsc_t struct that might carry width / height. */
const RE_LV_DSC_W = /\.w\s*=\s*(\d+)/;
const RE_LV_DSC_H = /\.h\s*=\s*(\d+)/;

/** Extracts individual numeric literals from a byte-array body. */
const RE_HEX   = /0x([0-9a-fA-F]{1,2})\b/g;
const RE_BIN   = /0b([01]+)\b/g;
const RE_DEC   = /\b([0-9]{1,3})\b/g;

// LVGL indexed-1bit palette is always 8 bytes (two ARGB8888 colour entries).
const LVGL_INDEXED_1BIT_PALETTE_BYTES = 8;

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/**
 * @typedef {Object} ParseResult
 * @property {string}     symbol          - Symbol name found in the file.
 * @property {Uint8Array} bytes           - Raw bytes (palette stripped for lvgl-indexed-1bit).
 * @property {Uint8Array|null} palette    - 8-byte palette for lvgl-indexed-1bit, else null.
 * @property {number}     rawByteCount    - Total bytes including palette (if any).
 * @property {number|null} inferredWidth  - Width inferred from lv_img_dsc_t header, or null.
 * @property {number|null} inferredHeight - Height inferred from lv_img_dsc_t header, or null.
 * @property {string[]}   warnings        - Non-fatal messages.
 * @property {string[]}   errors          - Fatal parse errors.
 */

/**
 * Parse a C source file and extract the named symbol's byte array.
 *
 * @param {string} source     - Full file contents as a string.
 * @param {string} symbolName - Name of the C symbol to extract (e.g. "placeholder_00_map").
 * @param {string} [format]   - Optional format hint. Pass "lvgl-indexed-1bit" to strip palette.
 * @returns {ParseResult}
 */
export function parseSymbol(source, symbolName, format = 'auto') {
  /** @type {ParseResult} */
  const result = {
    symbol: symbolName,
    bytes: new Uint8Array(0),
    palette: null,
    rawByteCount: 0,
    inferredWidth: null,
    inferredHeight: null,
    warnings: [],
    errors: [],
  };

  // --- Locate array declaration ---
  const arrayBody = extractArrayBody(source, symbolName);
  if (arrayBody === null) {
    result.errors.push(`Symbol "${symbolName}" not found in source file.`);
    return result;
  }

  // --- Parse bytes ---
  const rawBytes = parseByteLiterals(arrayBody, result.warnings);
  result.rawByteCount = rawBytes.length;

  if (rawBytes.length === 0) {
    result.errors.push(`Symbol "${symbolName}" found but no byte literals could be parsed from its body.`);
    return result;
  }

  // --- Infer dimensions from lv_img_dsc_t struct that references this symbol ---
  const { w, h } = inferLvglDimensions(source, symbolName);
  if (w !== null) result.inferredWidth  = w;
  if (h !== null) result.inferredHeight = h;

  // --- Determine if we should strip LVGL palette ---
  const isLvgl = format === 'lvgl-indexed-1bit' ||
    (format === 'auto' && looksLikeLvglIndexed1bit(source, symbolName));

  if (isLvgl) {
    if (rawBytes.length < LVGL_INDEXED_1BIT_PALETTE_BYTES) {
      result.warnings.push(
        `Expected at least ${LVGL_INDEXED_1BIT_PALETTE_BYTES} bytes for LVGL palette prefix, ` +
        `but only ${rawBytes.length} bytes found. Treating entire array as pixel data.`
      );
      result.bytes = rawBytes;
    } else {
      result.palette = rawBytes.slice(0, LVGL_INDEXED_1BIT_PALETTE_BYTES);
      result.bytes   = rawBytes.slice(LVGL_INDEXED_1BIT_PALETTE_BYTES);
    }
  } else {
    result.bytes = rawBytes;
  }

  return result;
}

/**
 * List all C array symbol names found in a source file.
 * Useful for auto-discovery.
 *
 * @param {string} source
 * @returns {string[]}
 */
export function listSymbols(source) {
  const symbols = [];
  const lines = source.split('\n');
  for (const line of lines) {
    const m = line.match(RE_ARRAY_DECL);
    if (m) symbols.push(m[1]);
  }
  return symbols;
}

// ---------------------------------------------------------------------------
// Internals
// ---------------------------------------------------------------------------

/**
 * Find the body of `symbol[] = { ... }` in source text.
 * Handles multi-line arrays. Returns null if not found.
 *
 * @param {string} source
 * @param {string} symbolName
 * @returns {string|null}
 */
function extractArrayBody(source, symbolName) {
  // Build a regex that looks for exactly this symbol name followed by [] = {
  // We allow optional "static", "const", "uint8_t" / "unsigned char" before it.
  const declPattern = new RegExp(
    String.raw`(?:static\s+)?(?:const\s+)?(?:uint8_t|unsigned\s+char)\s+` +
    escapeRegExp(symbolName) +
    String.raw`\s*\[\s*\d*\s*\]\s*=\s*\{`,
    'g'
  );

  const match = declPattern.exec(source);
  if (!match) return null;

  // Collect everything from after '{' until the matching '}'
  const start = match.index + match[0].length;
  let depth = 1;
  let i = start;
  while (i < source.length && depth > 0) {
    if (source[i] === '{') depth++;
    else if (source[i] === '}') depth--;
    i++;
  }

  return source.slice(start, i - 1); // exclude final '}'
}

/**
 * Parse all numeric byte literals from an array body string.
 * Handles 0x__, 0b__, and decimal.
 * Strips C-style comments before parsing.
 *
 * @param {string} body
 * @param {string[]} warnings
 * @returns {Uint8Array}
 */
function parseByteLiterals(body, warnings) {
  // Strip line comments and block comments
  const clean = body
    .replace(/\/\/[^\n]*/g, ' ')
    .replace(/\/\*[\s\S]*?\*\//g, ' ');

  const bytes = [];

  // We collect tokens in order: hex takes priority, then binary, then decimal.
  // To avoid double-matching, we tokenise the whole string once left-to-right.
  const tokenRe = /(0x[0-9a-fA-F]{1,2}|0b[01]+|\b\d{1,3})\b/g;
  let m;
  while ((m = tokenRe.exec(clean)) !== null) {
    const tok = m[1];
    let val;
    if (tok.startsWith('0x')) {
      val = parseInt(tok, 16);
    } else if (tok.startsWith('0b')) {
      val = parseInt(tok.slice(2), 2);
    } else {
      val = parseInt(tok, 10);
    }
    if (val < 0 || val > 255) {
      warnings.push(`Byte literal "${tok}" value ${val} out of 0–255 range; clamped.`);
      val = Math.max(0, Math.min(255, val));
    }
    bytes.push(val);
  }

  return new Uint8Array(bytes);
}

/**
 * Try to infer width and height from an lv_img_dsc_t struct that references `symbolName`
 * (i.e. look for `.data = symbolName` nearby and then find `.w = N` and `.h = M`).
 *
 * @param {string} source
 * @param {string} symbolName
 * @returns {{ w: number|null, h: number|null }}
 */
function inferLvglDimensions(source, symbolName) {
  // Look for the lv_img_dsc_t block that contains .data = symbolName
  // We search within a reasonable window around the data reference.
  const dataRef = new RegExp(String.raw`\.data\s*=\s*` + escapeRegExp(symbolName));
  const dm = dataRef.exec(source);
  if (!dm) return { w: null, h: null };

  // Search in a window of ~2000 chars around the reference
  const windowStart = Math.max(0, dm.index - 500);
  const windowEnd   = Math.min(source.length, dm.index + 1500);
  const window      = source.slice(windowStart, windowEnd);

  const wm = RE_LV_DSC_W.exec(window);
  const hm = RE_LV_DSC_H.exec(window);

  return {
    w: wm ? parseInt(wm[1], 10) : null,
    h: hm ? parseInt(hm[1], 10) : null,
  };
}

/**
 * Heuristic: detect if the source file uses LVGL indexed-1bit format for the given symbol.
 * Looks for LV_IMG_CF_INDEXED_1BIT anywhere in the same file, or for palette-shaped comments.
 *
 * @param {string} source
 * @param {string} symbolName
 * @returns {boolean}
 */
function looksLikeLvglIndexed1bit(source, symbolName) {
  return (
    source.includes('LV_IMG_CF_INDEXED_1BIT') ||
    source.includes('INDEXED_1BIT') ||
    source.includes('Color of index 0') ||
    source.includes('Color of index 1') ||
    /lv_img_dsc_t\s+/.test(source)
  );
}

/**
 * Escape a string for use inside a RegExp.
 *
 * @param {string} s
 * @returns {string}
 */
function escapeRegExp(s) {
  return s.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
}
