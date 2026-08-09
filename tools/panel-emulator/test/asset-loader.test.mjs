/**
 * test/asset-loader.test.mjs
 *
 * Tests for src/asset-loader.mjs using Node.js built-in test runner.
 * Uses a temporary directory with fixture files so no real repo files are required.
 */

import { describe, it, before, after } from 'node:test';
import assert from 'node:assert/strict';
import { mkdtemp, writeFile, rm } from 'node:fs/promises';
import { join }  from 'node:path';
import { tmpdir } from 'node:os';

import { loadAssets } from '../src/asset-loader.mjs';

// ---------------------------------------------------------------------------
// Fixtures
// ---------------------------------------------------------------------------

const GOOD_SOURCE = `
static const uint8_t my_sprite[] = {
  0xFF, 0x00, 0xFF, 0x00,
  0x00, 0xFF, 0x00, 0xFF
};
`;

// 8×8 source: 8 rows × 1 byte/row = 8 bytes
const SOURCE_8x8 = `
static const uint8_t bitmap_8x8[] = {
  0xFF, 0x00, 0xFF, 0x00,
  0xFF, 0x00, 0xFF, 0x00
};
`;

const SOURCE_WRONG_BYTES = `
/* Only 2 bytes instead of 8 for 8×8 */
static const uint8_t small[] = {
  0xFF, 0x00
};
`;

// ---------------------------------------------------------------------------
// Test suite setup: create a temp dir with fixture files
// ---------------------------------------------------------------------------

let tmpDir;
let goodSourcePath;
let source8x8Path;
let wrongBytesPath;
let assetsJsonPath;

before(async () => {
  tmpDir = await mkdtemp(join(tmpdir(), 'panel-emulator-test-'));

  goodSourcePath  = join(tmpDir, 'good.c');
  source8x8Path   = join(tmpDir, 'bitmap8x8.c');
  wrongBytesPath  = join(tmpDir, 'wrong.c');

  await writeFile(goodSourcePath,  GOOD_SOURCE, 'utf-8');
  await writeFile(source8x8Path,   SOURCE_8x8,  'utf-8');
  await writeFile(wrongBytesPath,  SOURCE_WRONG_BYTES, 'utf-8');

  assetsJsonPath = join(tmpDir, 'assets.json');
});

after(async () => {
  await rm(tmpDir, { recursive: true, force: true });
});

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

async function writeManifest(manifest) {
  await writeFile(assetsJsonPath, JSON.stringify(manifest, null, 2), 'utf-8');
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

describe('loadAssets — manifest parsing', () => {
  it('returns top-level error when assets.json does not exist', async () => {
    const r = await loadAssets(join(tmpDir, 'nonexistent.json'));
    assert.ok(r.errors.length > 0, 'should have a top-level error');
    assert.equal(r.assets.size, 0);
  });

  it('returns empty assets for an assets.json with no "assets" array', async () => {
    await writeFile(join(tmpDir, 'empty.json'), '{}', 'utf-8');
    const r = await loadAssets(join(tmpDir, 'empty.json'));
    assert.equal(r.assets.size, 0);
    assert.ok(r.warnings.length > 0);
  });

  it('reads screen dimensions from manifest', async () => {
    await writeManifest({ screen: { width: 128, height: 32 }, assets: [] });
    const r = await loadAssets(assetsJsonPath);
    assert.equal(r.screen.width,  128);
    assert.equal(r.screen.height, 32);
  });
});

describe('loadAssets — single well-formed asset', () => {
  it('loads and parses a well-formed asset', async () => {
    await writeManifest({
      screen: { width: 128, height: 64 },
      assets: [{
        name:   'good_sprite',
        source: 'good.c',  // relative to manifest dir
        symbol: 'my_sprite',
        width:  4,
        height: 2,
        frames: 1,
        format: 'mono-horizontal-msb',
      }],
    });
    const r = await loadAssets(assetsJsonPath);
    assert.equal(r.errors.length, 0, 'no top-level errors');
    const entry = r.assets.get('good_sprite');
    assert.ok(entry, 'asset entry should exist');
    assert.equal(entry.loaded,       true);
    assert.equal(entry.errors.length, 0);
    assert.equal(entry.rawByteCount,  8);
    assert.ok(entry.bytes instanceof Uint8Array);
    assert.equal(entry.bytes.length, 8);
  });

  it('resolves source path relative to assets.json directory (not cwd)', async () => {
    // The fixture is in tmpDir; if resolution used cwd it would fail
    await writeManifest({
      assets: [{
        name:   'path_test',
        source: 'good.c',  // relative
        symbol: 'my_sprite',
        width: 4, height: 2, frames: 1, format: 'mono-horizontal-msb',
      }],
    });
    const r = await loadAssets(assetsJsonPath);
    const entry = r.assets.get('path_test');
    assert.ok(entry.loaded, 'should be loaded via relative path');
  });
});

describe('loadAssets — byte count validation', () => {
  it('produces a warning when byte count matches declared dimensions', async () => {
    // 8 wide × 8 tall × 1 frame = 8 bytes. Source has exactly 8 bytes.
    await writeManifest({
      assets: [{
        name:   'exact',
        source: 'bitmap8x8.c',
        symbol: 'bitmap_8x8',
        width:  8,
        height: 8,
        frames: 1,
        format: 'mono-horizontal-msb',
      }],
    });
    const r = await loadAssets(assetsJsonPath);
    const entry = r.assets.get('exact');
    assert.ok(entry.loaded);
    // 8×8 horizontal-msb = 1 byte/row × 8 rows = 8 bytes. Source has 8 bytes → match.
    assert.equal(entry.warnings.filter(w => w.includes('expects')).length, 0, 'no byte-count mismatch');
  });

  it('produces a mismatch warning when byte count is wrong', async () => {
    await writeManifest({
      assets: [{
        name:   'mismatch',
        source: 'wrong.c',
        symbol: 'small',
        width:  8,   // declares 8×8 but only 2 bytes in source
        height: 8,
        frames: 1,
        format: 'mono-horizontal-msb',
      }],
    });
    const r = await loadAssets(assetsJsonPath);
    const entry = r.assets.get('mismatch');
    assert.ok(entry.loaded, 'still loaded (no crash)');
    assert.ok(
      entry.warnings.some(w => w.includes('expects')),
      'should have a mismatch warning'
    );
  });
});

describe('loadAssets — error conditions', () => {
  it('records an error per asset when source file does not exist', async () => {
    await writeManifest({
      assets: [{
        name:   'missing_file',
        source: 'nonexistent.c',
        symbol: 'some_symbol',
        width:  8, height: 8, frames: 1, format: 'mono-horizontal-msb',
      }],
    });
    const r = await loadAssets(assetsJsonPath);
    const entry = r.assets.get('missing_file');
    assert.ok(entry, 'entry should still exist');
    assert.equal(entry.loaded, false);
    assert.ok(entry.errors.length > 0, 'should have errors');
    // Top-level should not throw
    assert.equal(r.errors.length, 0, 'no top-level error; failure is per-asset');
  });

  it('records an error when symbol is not found in source file', async () => {
    await writeManifest({
      assets: [{
        name:   'bad_symbol',
        source: 'good.c',
        symbol: 'nonexistent_symbol',
        width:  4, height: 2, frames: 1, format: 'mono-horizontal-msb',
      }],
    });
    const r = await loadAssets(assetsJsonPath);
    const entry = r.assets.get('bad_symbol');
    assert.equal(entry.loaded, false);
    assert.ok(entry.errors.some(e => /not found/i.test(e)));
  });

  it('processes remaining assets even when one asset fails', async () => {
    await writeManifest({
      assets: [
        { name: 'bad',  source: 'nonexistent.c', symbol: 'x', width: 1, height: 1, frames: 1, format: 'mono-horizontal-msb' },
        { name: 'good', source: 'good.c', symbol: 'my_sprite', width: 4, height: 2, frames: 1, format: 'mono-horizontal-msb' },
      ],
    });
    const r = await loadAssets(assetsJsonPath);
    assert.equal(r.assets.get('bad').loaded,  false);
    assert.equal(r.assets.get('good').loaded, true);
  });
});
