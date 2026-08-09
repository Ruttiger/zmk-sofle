/**
 * test/bitmap-renderer.test.mjs
 *
 * Tests for src/bitmap-renderer.mjs using Node.js built-in test runner.
 */

import { describe, it } from 'node:test';
import assert from 'node:assert/strict';
import { renderBitmap, expectedByteCount } from '../src/bitmap-renderer.mjs';

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/**
 * Build a mono-horizontal-msb byte array from a string of '0'/'1' chars.
 * Width is inferred from the first row length.
 *
 * @param {string[]} rows  e.g. ['10101010', '01010101']
 * @returns {{ bytes: Uint8Array, width: number, height: number }}
 */
function makeHorizMSB(rows) {
  const width  = rows[0].length;
  const height = rows.length;
  const stride = Math.ceil(width / 8);
  const bytes  = new Uint8Array(stride * height);

  for (let r = 0; r < height; r++) {
    for (let c = 0; c < width; c++) {
      if (rows[r][c] === '1') {
        bytes[r * stride + Math.floor(c / 8)] |= (1 << (7 - (c % 8)));
      }
    }
  }
  return { bytes, width, height };
}

/**
 * Convert flat pixel array to a 2D string grid for easy comparison.
 * @param {Uint8Array} pixels
 * @param {number}     width
 * @returns {string[]}
 */
function pixelsToGrid(pixels, width) {
  const rows = [];
  for (let r = 0; r < pixels.length / width; r++) {
    rows.push(
      Array.from(pixels.slice(r * width, (r + 1) * width))
           .map(v => v ? '1' : '0')
           .join('')
    );
  }
  return rows;
}

// ---------------------------------------------------------------------------
// Tests — mono-horizontal-msb
// ---------------------------------------------------------------------------

describe('renderBitmap — mono-horizontal-msb', () => {
  it('renders a known 8×1 pattern correctly', () => {
    const { bytes, width, height } = makeHorizMSB(['10101010']);
    const { pixels, warnings } = renderBitmap(bytes, width, height, 'mono-horizontal-msb');
    assert.equal(warnings.length, 0);
    const grid = pixelsToGrid(pixels, width);
    assert.deepEqual(grid, ['10101010']);
  });

  it('renders a known 8×2 checkerboard correctly', () => {
    const { bytes, width, height } = makeHorizMSB(['10101010', '01010101']);
    const { pixels } = renderBitmap(bytes, width, height, 'mono-horizontal-msb');
    const grid = pixelsToGrid(pixels, width);
    assert.deepEqual(grid, ['10101010', '01010101']);
  });

  it('renders a 4×4 pattern correctly', () => {
    const rows = ['1100', '0011', '1010', '0101'];
    const { bytes, width, height } = makeHorizMSB(rows);
    const { pixels } = renderBitmap(bytes, width, height, 'mono-horizontal-msb');
    assert.deepEqual(pixelsToGrid(pixels, width), rows);
  });

  it('returns a partial image and a warning when bytes are insufficient', () => {
    const { width, height } = makeHorizMSB(['11111111', '11111111']);
    const shortBytes = new Uint8Array(1); // only one byte instead of 2
    shortBytes[0] = 0b10110101;
    const { pixels, warnings } = renderBitmap(shortBytes, width, height, 'mono-horizontal-msb');
    assert.ok(warnings.length > 0, 'expected a warning');
    // First row should decode from the one byte we gave
    assert.equal(pixels[0], 255); // bit 7 of 0b10110101 = 1
  });
});

// ---------------------------------------------------------------------------
// Tests — mono-horizontal-lsb vs mono-horizontal-msb (bit order inversion)
// ---------------------------------------------------------------------------

describe('renderBitmap — mono-horizontal-lsb bit order', () => {
  it('interprets bits in opposite order from MSB for the same byte', () => {
    // Single byte 0b10000000 = 0x80
    // MSB: bit7=1 → pixel0=on, rest off → "10000000"
    // LSB: bit0=0 → pixel0=off, rest off → "00000001"
    const bytes = new Uint8Array([0b10000001]);
    const { pixels: msbPx } = renderBitmap(bytes, 8, 1, 'mono-horizontal-msb');
    const { pixels: lsbPx } = renderBitmap(bytes, 8, 1, 'mono-horizontal-lsb');

    // MSB: leftmost pixel = bit7 of 0b10000001 = 1
    assert.equal(msbPx[0], 255, 'MSB: pixel 0 should be on');
    assert.equal(msbPx[7], 255, 'MSB: pixel 7 should be on');

    // LSB: leftmost pixel = bit0 of 0b10000001 = 1
    assert.equal(lsbPx[0], 255, 'LSB: pixel 0 should be on');
    assert.equal(lsbPx[7], 255, 'LSB: pixel 7 should be on');

    // Middle pixels differ
    assert.equal(msbPx[1], 0,   'MSB: pixel 1 off');
    assert.equal(lsbPx[1], 0,   'LSB: pixel 1 off');
  });

  it('same bytes render differently for MSB vs LSB', () => {
    // 0b11110000 = 0xF0
    // MSB: 11110000
    // LSB: 00001111
    const bytes = new Uint8Array([0b11110000]);
    const { pixels: msb } = renderBitmap(bytes, 8, 1, 'mono-horizontal-msb');
    const { pixels: lsb } = renderBitmap(bytes, 8, 1, 'mono-horizontal-lsb');

    assert.deepEqual([...msb], [255,255,255,255,0,0,0,0]);
    assert.deepEqual([...lsb], [0,0,0,0,255,255,255,255]);
  });
});

// ---------------------------------------------------------------------------
// Tests — mono-vertical-pages-lsb
// ---------------------------------------------------------------------------

describe('renderBitmap — mono-vertical-pages-lsb', () => {
  it('places the first page column bytes into the correct rows', () => {
    // 8×8 canvas, 1 page. One column byte: 0b10000001
    // LSB: bit0=page_row0, bit7=page_row7
    // So: row0=1, row1=0, ..., row6=0, row7=1
    const bytes = new Uint8Array(8); // 8 columns in the one page
    bytes[0] = 0b10000001; // only first column is set

    const { pixels } = renderBitmap(bytes, 8, 8, 'mono-vertical-pages-lsb');
    assert.equal(pixels[0 * 8 + 0], 255, 'row 0, col 0 on');
    assert.equal(pixels[7 * 8 + 0], 255, 'row 7, col 0 on');
    assert.equal(pixels[1 * 8 + 0],   0, 'row 1, col 0 off');
    assert.equal(pixels[0 * 8 + 1],   0, 'row 0, col 1 off (second column is 0x00)');
  });

  it('handles 8×16 (two pages)', () => {
    // 8 wide, 16 tall = 2 pages × 8 bytes
    const bytes = new Uint8Array(16); // two pages
    bytes[0]  = 0xFF; // first byte of page 0, col 0 → all 8 rows in page 0, col 0 are on
    bytes[8]  = 0xFF; // first byte of page 1, col 0 → all 8 rows in page 1, col 0 are on

    const { pixels } = renderBitmap(bytes, 8, 16, 'mono-vertical-pages-lsb');
    // All pixels in column 0 should be on
    for (let row = 0; row < 16; row++) {
      assert.equal(pixels[row * 8 + 0], 255, `row ${row}, col 0 should be on`);
    }
    // Column 1 should all be off
    for (let row = 0; row < 16; row++) {
      assert.equal(pixels[row * 8 + 1], 0, `row ${row}, col 1 should be off`);
    }
  });
});

// ---------------------------------------------------------------------------
// Tests — options
// ---------------------------------------------------------------------------

describe('renderBitmap — invertColor', () => {
  it('inverts all pixel values', () => {
    const bytes = new Uint8Array([0xFF]); // all 8 pixels on
    const { pixels: normal } = renderBitmap(bytes, 8, 1, 'mono-horizontal-msb');
    const { pixels: inv   } = renderBitmap(bytes, 8, 1, 'mono-horizontal-msb', { invertColor: true });
    assert.ok(normal.every(v => v === 255));
    assert.ok(inv.every(v => v === 0));
  });
});

describe('renderBitmap — mirrorH', () => {
  it('horizontally flips pixels', () => {
    // Row = "10000000" → after mirrorH = "00000001"
    const bytes = new Uint8Array([0b10000000]);
    const { pixels } = renderBitmap(bytes, 8, 1, 'mono-horizontal-msb', { mirrorH: true });
    assert.equal(pixels[0], 0,   'first pixel should be off after mirrorH');
    assert.equal(pixels[7], 255, 'last pixel should be on after mirrorH');
  });
});

describe('renderBitmap — mirrorV', () => {
  it('vertically flips rows', () => {
    const { bytes, width, height } = makeHorizMSB(['11110000', '00001111']);
    const { pixels } = renderBitmap(bytes, width, height, 'mono-horizontal-msb', { mirrorV: true });
    const grid = pixelsToGrid(pixels, width);
    // Rows should be reversed
    assert.deepEqual(grid[0], '00001111');
    assert.deepEqual(grid[1], '11110000');
  });
});

// ---------------------------------------------------------------------------
// Tests — expectedByteCount
// ---------------------------------------------------------------------------

describe('expectedByteCount', () => {
  it('horizontal 8×8 = 8 bytes', () => {
    assert.equal(expectedByteCount(8, 8, 1, 'mono-horizontal-msb'), 8);
  });

  it('horizontal 10×4 = ceil(10/8)*4 = 8 bytes', () => {
    assert.equal(expectedByteCount(10, 4, 1, 'mono-horizontal-msb'), 8);
  });

  it('vertical-pages 8×8 = 1 page * 8 cols = 8 bytes', () => {
    assert.equal(expectedByteCount(8, 8, 1, 'mono-vertical-pages-lsb'), 8);
  });

  it('vertical-pages 8×16 = 2 pages * 8 cols = 16 bytes', () => {
    assert.equal(expectedByteCount(8, 16, 1, 'mono-vertical-pages-lsb'), 16);
  });

  it('multi-frame multiplies correctly', () => {
    assert.equal(expectedByteCount(8, 8, 4, 'mono-horizontal-msb'), 32);
  });

  it('mono-packed-rows aliases mono-horizontal-msb', () => {
    assert.equal(
      expectedByteCount(8, 8, 1, 'mono-packed-rows'),
      expectedByteCount(8, 8, 1, 'mono-horizontal-msb')
    );
  });
});
