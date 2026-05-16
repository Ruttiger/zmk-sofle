/**
 * test/asset-parser.test.mjs
 *
 * Tests for src/asset-parser.mjs using Node.js built-in test runner.
 * Run with: node --test test/asset-parser.test.mjs
 */

import { describe, it } from 'node:test';
import assert from 'node:assert/strict';
import { parseSymbol, listSymbols } from '../src/asset-parser.mjs';

// ---------------------------------------------------------------------------
// Fixtures
// ---------------------------------------------------------------------------

const SIMPLE_UINT8 = `
static const uint8_t my_icon[] = {
  0x3C, 0x42, 0x81, 0xFF
};
`;

const UNSIGNED_CHAR = `
const unsigned char battery_icon[] = {
  0xff, 0x81, 0x81, 0xff
};
`;

const BINARY_ARRAY = `
static const uint8_t sprite[] = {
  0b00011000,
  0b00111100,
  0b01111110
};
`;

const MULTI_SYMBOL = `
static const uint8_t icon_a[] = { 0x01, 0x02 };
static const uint8_t icon_b[] = { 0xAA, 0xBB };
`;

const NO_ARRAY = `
/* empty file */
int x = 42;
`;

const MIXED_HEX_BIN_DEC = `
static const uint8_t mixed[] = {
  0xFF,
  0b11001100,
  128
};
`;

// LVGL indexed-1bit: 8-byte palette + pixel data
const LVGL_INDEXED_1BIT = `
/* LV_IMG_CF_INDEXED_1BIT */
static const uint8_t my_frame_map[] = {
  /* palette (8 bytes) */
  0x00, 0x00, 0x00, 0xFF,  /* Color of index 0 (black) */
  0xFF, 0xFF, 0xFF, 0xFF,  /* Color of index 1 (white) */
  /* pixel data */
  0xAA, 0xBB
};
const lv_img_dsc_t my_frame = {
  .header = { .cf = LV_IMG_CF_INDEXED_1BIT, .w = 8, .h = 2 },
  .data_size = 10,
  .data = my_frame_map
};
`;

const LVGL_INDEXED_1BIT_2 = `
static const uint8_t my_frame_map[] = {
  0x00, 0x00, 0x00, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF,
  0xAA, 0xBB
};
`;

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

describe('parseSymbol — basic types', () => {
  it('parses uint8_t array with hex literals', () => {
    const r = parseSymbol(SIMPLE_UINT8, 'my_icon');
    assert.equal(r.errors.length, 0, 'no errors');
    assert.equal(r.rawByteCount, 4);
    assert.deepEqual([...r.bytes], [0x3C, 0x42, 0x81, 0xFF]);
  });

  it('parses unsigned char array with hex literals', () => {
    const r = parseSymbol(UNSIGNED_CHAR, 'battery_icon');
    assert.equal(r.errors.length, 0);
    assert.equal(r.rawByteCount, 4);
    assert.deepEqual([...r.bytes], [0xFF, 0x81, 0x81, 0xFF]);
  });

  it('parses binary literals (0b...)', () => {
    const r = parseSymbol(BINARY_ARRAY, 'sprite');
    assert.equal(r.errors.length, 0);
    assert.equal(r.rawByteCount, 3);
    assert.equal(r.bytes[0], 0b00011000);
    assert.equal(r.bytes[1], 0b00111100);
    assert.equal(r.bytes[2], 0b01111110);
  });

  it('parses mixed hex, binary and decimal literals', () => {
    const r = parseSymbol(MIXED_HEX_BIN_DEC, 'mixed');
    assert.equal(r.errors.length, 0);
    assert.equal(r.rawByteCount, 3);
    assert.equal(r.bytes[0], 0xFF);
    assert.equal(r.bytes[1], 0b11001100);
    assert.equal(r.bytes[2], 128);
  });
});

describe('parseSymbol — multi-symbol and not-found', () => {
  it('extracts only the requested symbol from a multi-symbol file', () => {
    const ra = parseSymbol(MULTI_SYMBOL, 'icon_a');
    assert.equal(ra.errors.length, 0);
    assert.deepEqual([...ra.bytes], [0x01, 0x02]);

    const rb = parseSymbol(MULTI_SYMBOL, 'icon_b');
    assert.equal(rb.errors.length, 0);
    assert.deepEqual([...rb.bytes], [0xAA, 0xBB]);
  });

  it('returns an error when symbol is not found', () => {
    const r = parseSymbol(NO_ARRAY, 'missing_symbol');
    assert.equal(r.errors.length, 1);
    assert.match(r.errors[0], /not found/i);
    assert.equal(r.bytes.length, 0);
  });

  it('returns an error for a symbol that exists by wrong name', () => {
    const r = parseSymbol(SIMPLE_UINT8, 'wrong_name');
    assert.equal(r.errors.length, 1);
  });
});

describe('parseSymbol — LVGL indexed-1bit', () => {
  it('strips 8-byte palette prefix for lvgl-indexed-1bit format', () => {
    const r = parseSymbol(LVGL_INDEXED_1BIT, 'my_frame_map', 'lvgl-indexed-1bit');
    assert.equal(r.errors.length, 0, 'no errors');
    assert.equal(r.rawByteCount, 10, 'total 10 bytes');
    assert.equal(r.palette?.length, 8, 'palette = 8 bytes');
    assert.equal(r.bytes.length, 2, 'pixel data = 2 bytes');
    assert.deepEqual([...r.bytes], [0xAA, 0xBB]);
  });

  it('auto-detects LVGL format via LV_IMG_CF_INDEXED_1BIT comment', () => {
    const r = parseSymbol(LVGL_INDEXED_1BIT, 'my_frame_map', 'auto');
    assert.equal(r.errors.length, 0);
    assert.equal(r.palette?.length, 8);
    assert.equal(r.bytes.length, 2);
  });

  it('infers width and height from lv_img_dsc_t header', () => {
    const r = parseSymbol(LVGL_INDEXED_1BIT, 'my_frame_map', 'auto');
    assert.equal(r.inferredWidth,  8);
    assert.equal(r.inferredHeight, 2);
  });
});

describe('listSymbols', () => {
  it('lists all array symbols in a file', () => {
    const syms = listSymbols(MULTI_SYMBOL);
    assert.ok(syms.includes('icon_a'));
    assert.ok(syms.includes('icon_b'));
  });

  it('returns empty array for a file with no arrays', () => {
    const syms = listSymbols(NO_ARRAY);
    assert.equal(syms.length, 0);
  });
});
