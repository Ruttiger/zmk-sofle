/**
 * bitmap-renderer.mjs
 *
 * Pure function: convert a raw byte array into a flat pixel array (one byte per pixel,
 * 0 = off, 255 = on) using a selectable bitmap format.
 *
 * Supported formats:
 *
 *   lvgl-indexed-1bit   — LVGL 1bpp horizontal, MSB first, row-padded to byte boundary.
 *                         Palette already stripped by asset-parser. Identical encoding
 *                         to mono-horizontal-msb.
 *   mono-horizontal-msb — 1bpp, row by row, MSB of first byte = leftmost pixel.
 *   mono-horizontal-lsb — 1bpp, row by row, LSB of first byte = leftmost pixel.
 *   mono-vertical-pages-lsb — OLED page-addressed: bytes are columns within 8px-tall pages.
 *                              Byte N = column (N mod w), page (N / w). Bit 0 = topmost pixel.
 *   mono-vertical-pages-msb — Same layout but Bit 7 = topmost pixel.
 *   mono-packed-rows        — Alias for mono-horizontal-msb.
 *   mono-packed-columns     — Alias for mono-vertical-pages-lsb.
 *
 * Post-decode transforms (applied in this order):
 *   invertColor  — swap 0 ↔ 255
 *   mirrorH      — flip each row left-right
 *   mirrorV      — flip rows top-bottom
 *   rotation     — 0 / 90 / 180 / 270 degrees clockwise
 *
 * @module bitmap-renderer
 */

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/**
 * @typedef {Object} RenderOptions
 * @property {boolean} [invertColor=false]
 * @property {boolean} [mirrorH=false]
 * @property {boolean} [mirrorV=false]
 * @property {0|90|180|270} [rotation=0]
 */

/**
 * @typedef {Object} RenderResult
 * @property {Uint8Array} pixels  - Flat array, one byte per pixel: 0=off, 255=on.
 * @property {number}     width   - Output width (may differ from input if rotated 90/270).
 * @property {number}     height  - Output height.
 * @property {string[]}   warnings
 */

/**
 * Render a byte array as a monochrome bitmap.
 *
 * @param {Uint8Array} bytes
 * @param {number}     width    - Declared pixel width.
 * @param {number}     height   - Declared pixel height.
 * @param {string}     [format='mono-horizontal-msb']
 * @param {RenderOptions} [options={}]
 * @returns {RenderResult}
 */
export function renderBitmap(bytes, width, height, format = 'mono-horizontal-msb', options = {}) {
  const warnings = [];
  const { invertColor = false, mirrorH = false, mirrorV = false, rotation = 0 } = options;

  if (width <= 0 || height <= 0) {
    return { pixels: new Uint8Array(0), width: 0, height: 0, warnings: ['width and height must be > 0'] };
  }

  // Normalise format aliases
  const fmt = normaliseFormat(format);

  // Decode to flat pixel array
  let pixels;
  switch (fmt) {
    case 'mono-horizontal-msb':
    case 'lvgl-indexed-1bit':
      pixels = decodeHorizontal(bytes, width, height, 'msb', warnings);
      break;
    case 'mono-horizontal-lsb':
      pixels = decodeHorizontal(bytes, width, height, 'lsb', warnings);
      break;
    case 'mono-vertical-pages-lsb':
      pixels = decodeVerticalPages(bytes, width, height, 'lsb', warnings);
      break;
    case 'mono-vertical-pages-msb':
      pixels = decodeVerticalPages(bytes, width, height, 'msb', warnings);
      break;
    default:
      warnings.push(`Unknown format "${format}"; falling back to mono-horizontal-msb.`);
      pixels = decodeHorizontal(bytes, width, height, 'msb', warnings);
  }

  let w = width;
  let h = height;

  // Post-decode transforms
  if (invertColor) {
    for (let i = 0; i < pixels.length; i++) {
      pixels[i] = pixels[i] === 0 ? 255 : 0;
    }
  }

  if (mirrorH) {
    pixels = applyMirrorH(pixels, w, h);
  }

  if (mirrorV) {
    pixels = applyMirrorV(pixels, w, h);
  }

  if (rotation === 90 || rotation === 270) {
    ({ pixels, width: w, height: h } = applyRotation(pixels, w, h, rotation));
  } else if (rotation === 180) {
    pixels = applyMirrorH(applyMirrorV(pixels, w, h), w, h);
  }

  return { pixels, width: w, height: h, warnings };
}

/**
 * Calculate the expected byte count for a given width × height × frames in a given format.
 *
 * @param {number} width
 * @param {number} height
 * @param {number} [frames=1]
 * @param {string} [format='mono-horizontal-msb']
 * @returns {number}
 */
export function expectedByteCount(width, height, frames = 1, format = 'mono-horizontal-msb') {
  const fmt = normaliseFormat(format);
  const stride = Math.ceil(width / 8);

  switch (fmt) {
    case 'mono-horizontal-msb':
    case 'mono-horizontal-lsb':
    case 'lvgl-indexed-1bit':
      // Row-by-row; each row is ceil(width/8) bytes.
      return stride * height * frames;

    case 'mono-vertical-pages-lsb':
    case 'mono-vertical-pages-msb': {
      // Pages: ceil(height/8) pages, each page is `width` bytes.
      const pages = Math.ceil(height / 8);
      return pages * width * frames;
    }

    default:
      return Math.ceil(width / 8) * height * frames;
  }
}

// ---------------------------------------------------------------------------
// Decoders
// ---------------------------------------------------------------------------

/**
 * Decode horizontal packed bitmap (row-major, each row padded to byte boundary).
 *
 * @param {Uint8Array} bytes
 * @param {number}     width
 * @param {number}     height
 * @param {'msb'|'lsb'} bitOrder
 * @param {string[]}   warnings
 * @returns {Uint8Array} flat pixel array
 */
function decodeHorizontal(bytes, width, height, bitOrder, warnings) {
  const stride = Math.ceil(width / 8);
  const expectedBytes = stride * height;
  const pixels = new Uint8Array(width * height); // 0-filled = off

  if (bytes.length < expectedBytes) {
    warnings.push(
      `Horizontal decode: expected ${expectedBytes} bytes for ${width}×${height}, ` +
      `got ${bytes.length}. Image will be partially filled.`
    );
  }

  for (let row = 0; row < height; row++) {
    for (let col = 0; col < width; col++) {
      const byteIndex = row * stride + Math.floor(col / 8);
      if (byteIndex >= bytes.length) break;

      const bitPos = col % 8;
      const byte   = bytes[byteIndex];
      let bit;
      if (bitOrder === 'msb') {
        bit = (byte >> (7 - bitPos)) & 1;
      } else {
        bit = (byte >> bitPos) & 1;
      }
      pixels[row * width + col] = bit ? 255 : 0;
    }
  }

  return pixels;
}

/**
 * Decode vertical-page bitmap (OLED SSD1306-style page addressing).
 * Pages are ceil(height/8) tall, each page is `width` bytes wide.
 * Within each byte, bit N corresponds to the pixel at y = (pageIndex*8 + N).
 *
 * @param {Uint8Array} bytes
 * @param {number}     width
 * @param {number}     height
 * @param {'msb'|'lsb'} bitOrder  lsb: bit0=top; msb: bit7=top
 * @param {string[]}   warnings
 * @returns {Uint8Array}
 */
function decodeVerticalPages(bytes, width, height, bitOrder, warnings) {
  const pages = Math.ceil(height / 8);
  const expectedBytes = pages * width;
  const pixels = new Uint8Array(width * height);

  if (bytes.length < expectedBytes) {
    warnings.push(
      `Vertical-pages decode: expected ${expectedBytes} bytes for ${width}×${height}, ` +
      `got ${bytes.length}. Image will be partially filled.`
    );
  }

  for (let page = 0; page < pages; page++) {
    for (let col = 0; col < width; col++) {
      const byteIndex = page * width + col;
      if (byteIndex >= bytes.length) break;
      const byte = bytes[byteIndex];

      for (let bit = 0; bit < 8; bit++) {
        const row = page * 8 + (bitOrder === 'lsb' ? bit : 7 - bit);
        if (row >= height) continue;
        const pixelBit = (byte >> bit) & 1;
        pixels[row * width + col] = pixelBit ? 255 : 0;
      }
    }
  }

  return pixels;
}

// ---------------------------------------------------------------------------
// Post-decode transforms
// ---------------------------------------------------------------------------

function applyMirrorH(pixels, width, height) {
  const out = new Uint8Array(pixels.length);
  for (let row = 0; row < height; row++) {
    for (let col = 0; col < width; col++) {
      out[row * width + col] = pixels[row * width + (width - 1 - col)];
    }
  }
  return out;
}

function applyMirrorV(pixels, width, height) {
  const out = new Uint8Array(pixels.length);
  for (let row = 0; row < height; row++) {
    const srcRow = height - 1 - row;
    out.set(pixels.slice(srcRow * width, srcRow * width + width), row * width);
  }
  return out;
}

/**
 * Rotate 90° or 270° clockwise. Returns new { pixels, width, height }.
 *
 * @param {Uint8Array} pixels
 * @param {number}     width
 * @param {number}     height
 * @param {90|270}     degrees
 */
function applyRotation(pixels, width, height, degrees) {
  const newW = height;
  const newH = width;
  const out  = new Uint8Array(newW * newH);

  for (let row = 0; row < height; row++) {
    for (let col = 0; col < width; col++) {
      const src = row * width + col;
      let dstRow, dstCol;
      if (degrees === 90) {
        // (row, col) → (col, height-1-row)
        dstRow = col;
        dstCol = height - 1 - row;
      } else {
        // 270: (row, col) → (width-1-col, row)
        dstRow = width - 1 - col;
        dstCol = row;
      }
      out[dstRow * newW + dstCol] = pixels[src];
    }
  }

  return { pixels: out, width: newW, height: newH };
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

function normaliseFormat(fmt) {
  switch (fmt) {
    case 'mono-packed-rows':    return 'mono-horizontal-msb';
    case 'mono-packed-columns': return 'mono-vertical-pages-lsb';
    default:                    return fmt;
  }
}
