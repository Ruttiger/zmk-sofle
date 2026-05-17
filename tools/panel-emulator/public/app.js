/**
 * app.js — OLED Panel Emulator client
 *
 * Vanilla ES module. No build step. Runs entirely in the browser.
 *
 * Architecture:
 *   api          — fetch wrappers for server endpoints
 *   BitmapDecoder — client-side bitmap interpreter (mirrors bitmap-renderer.mjs)
 *   OLEDCanvas   — HTML5 Canvas renderer for a simulated monochrome OLED screen
 *   AssetViewer  — Asset Viewer tab logic
 *   PanelPreview — Panel Preview tab logic
 *   StatePanel   — Keyboard State tab logic
 */

'use strict';

// ===========================================================================
// API client
// ===========================================================================

const API = (() => {
  const BASE = '';  // same origin

  async function get(path) {
    const r = await fetch(BASE + path);
    if (!r.ok) throw new Error(`GET ${path} → ${r.status}`);
    return r.json();
  }

  async function post(path, body) {
    const r = await fetch(BASE + path, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(body ?? {}),
    });
    if (!r.ok) throw new Error(`POST ${path} → ${r.status}`);
    return r.json();
  }

  async function patch(path, body) {
    const r = await fetch(BASE + path, {
      method: 'PATCH',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(body),
    });
    if (!r.ok) throw new Error(`PATCH ${path} → ${r.status}`);
    return r.json();
  }

  return {
    getAssets:     ()       => get('/api/assets'),
    getAsset:      (name)   => get(`/api/assets/${encodeURIComponent(name)}`),
    getState:      ()       => get('/api/state'),
    patchState:    (body)   => patch('/api/state', body),
    postKeyEvent:  (event)  => post('/api/key-event', event),
    reloadAssets:  ()       => post('/api/reload-assets'),
    resetState:    ()       => post('/api/reset'),
  };
})();

// ===========================================================================
// Bitmap decoder (client-side mirror of bitmap-renderer.mjs)
// ===========================================================================

const BitmapDecoder = (() => {

  /**
   * Decode a Uint8Array to a flat pixel array (0=off, 255=on).
   *
   * @param {Uint8Array} bytes
   * @param {number}     width
   * @param {number}     height
   * @param {string}     format
   * @param {object}     opts  { invertColor, mirrorH, mirrorV, rotation }
   * @returns {{ pixels: Uint8Array, width: number, height: number, warnings: string[] }}
   */
  function renderBitmap(bytes, width, height, format, opts = {}) {
    const warnings = [];
    const { invertColor = false, mirrorH = false, mirrorV = false, rotation = 0 } = opts;

    if (!bytes || width <= 0 || height <= 0) {
      return { pixels: new Uint8Array(0), width, height, warnings: ['Invalid dimensions'] };
    }

    const fmt = normalise(format);

    let pixels;
    switch (fmt) {
      case 'mono-horizontal-msb':
      case 'lvgl-indexed-1bit':
        pixels = decodeHorizMSB(bytes, width, height, warnings); break;
      case 'mono-horizontal-lsb':
        pixels = decodeHorizLSB(bytes, width, height, warnings); break;
      case 'mono-vertical-pages-lsb':
        pixels = decodePages(bytes, width, height, 'lsb', warnings); break;
      case 'mono-vertical-pages-msb':
        pixels = decodePages(bytes, width, height, 'msb', warnings); break;
      default:
        warnings.push(`Unknown format "${format}"; using mono-horizontal-msb.`);
        pixels = decodeHorizMSB(bytes, width, height, warnings);
    }

    let w = width, h = height;
    if (invertColor) for (let i = 0; i < pixels.length; i++) pixels[i] ^= 255;
    if (mirrorH) pixels = flipH(pixels, w, h);
    if (mirrorV) pixels = flipV(pixels, w, h);
    if (rotation === 90 || rotation === 270) ({ pixels, width: w, height: h } = rotate(pixels, w, h, rotation));
    else if (rotation === 180) { pixels = flipH(flipV(pixels, w, h), w, h); }

    return { pixels, width: w, height: h, warnings };
  }

  function normalise(fmt) {
    if (fmt === 'mono-packed-rows')    return 'mono-horizontal-msb';
    if (fmt === 'mono-packed-columns') return 'mono-vertical-pages-lsb';
    return fmt;
  }

  function decodeHorizMSB(bytes, w, h, warns) {
    const stride = Math.ceil(w / 8);
    const px = new Uint8Array(w * h);
    if (bytes.length < stride * h) warns.push(`Only ${bytes.length} bytes for ${w}×${h} (need ${stride * h}); partial render.`);
    for (let row = 0; row < h; row++)
      for (let col = 0; col < w; col++) {
        const bi = row * stride + (col >> 3);
        if (bi >= bytes.length) break;
        px[row * w + col] = ((bytes[bi] >> (7 - (col & 7))) & 1) ? 255 : 0;
      }
    return px;
  }

  function decodeHorizLSB(bytes, w, h, warns) {
    const stride = Math.ceil(w / 8);
    const px = new Uint8Array(w * h);
    if (bytes.length < stride * h) warns.push(`Only ${bytes.length} bytes; partial render.`);
    for (let row = 0; row < h; row++)
      for (let col = 0; col < w; col++) {
        const bi = row * stride + (col >> 3);
        if (bi >= bytes.length) break;
        px[row * w + col] = ((bytes[bi] >> (col & 7)) & 1) ? 255 : 0;
      }
    return px;
  }

  function decodePages(bytes, w, h, bitOrder, warns) {
    const pages = Math.ceil(h / 8);
    const px = new Uint8Array(w * h);
    if (bytes.length < pages * w) warns.push(`Only ${bytes.length} bytes; partial render.`);
    for (let pg = 0; pg < pages; pg++)
      for (let col = 0; col < w; col++) {
        const bi = pg * w + col;
        if (bi >= bytes.length) break;
        const byte = bytes[bi];
        for (let bit = 0; bit < 8; bit++) {
          const row = pg * 8 + (bitOrder === 'lsb' ? bit : 7 - bit);
          if (row >= h) continue;
          px[row * w + col] = ((byte >> bit) & 1) ? 255 : 0;
        }
      }
    return px;
  }

  function flipH(px, w, h) {
    const out = new Uint8Array(px.length);
    for (let r = 0; r < h; r++)
      for (let c = 0; c < w; c++)
        out[r * w + c] = px[r * w + (w - 1 - c)];
    return out;
  }

  function flipV(px, w, h) {
    const out = new Uint8Array(px.length);
    for (let r = 0; r < h; r++)
      out.set(px.slice((h - 1 - r) * w, (h - r) * w), r * w);
    return out;
  }

  function rotate(px, w, h, deg) {
    const nw = h, nh = w;
    const out = new Uint8Array(nw * nh);
    for (let r = 0; r < h; r++)
      for (let c = 0; c < w; c++) {
        const src = r * w + c;
        let dr, dc;
        if (deg === 90)  { dr = c;         dc = h - 1 - r; }
        else              { dr = w - 1 - c; dc = r;         }
        out[dr * nw + dc] = px[src];
      }
    return { pixels: out, width: nw, height: nh };
  }

  /** Base64 string → Uint8Array */
  function b64ToBytes(b64) {
    const bin = atob(b64);
    const out = new Uint8Array(bin.length);
    for (let i = 0; i < bin.length; i++) out[i] = bin.charCodeAt(i);
    return out;
  }

  return { renderBitmap, b64ToBytes };
})();

// ===========================================================================
// OLED Canvas renderer
// ===========================================================================

class OLEDCanvas {
  /**
   * @param {HTMLCanvasElement} canvas
   * @param {object} [opts]
   */
  constructor(canvas, opts = {}) {
    this.canvas  = canvas;
    this.ctx     = canvas.getContext('2d');
    this.zoom    = opts.zoom    ?? 4;
    this.colorOn = opts.colorOn ?? '#00ff88';
    this.colorBg = opts.colorBg ?? '#000000';
    this.grid    = opts.grid    ?? false;
    this.bbox    = opts.bbox    ?? false;
    this._scrW   = opts.screenW ?? 128;
    this._scrH   = opts.screenH ?? 64;
    this._resize();
  }

  _resize() {
    this.canvas.width  = this._scrW * this.zoom;
    this.canvas.height = this._scrH * this.zoom;
  }

  setOptions(opts) {
    if (opts.zoom    !== undefined) this.zoom    = opts.zoom;
    if (opts.colorOn !== undefined) this.colorOn = opts.colorOn;
    if (opts.colorBg !== undefined) this.colorBg = opts.colorBg;
    if (opts.grid    !== undefined) this.grid    = opts.grid;
    if (opts.bbox    !== undefined) this.bbox    = opts.bbox;
    if (opts.screenW !== undefined) this._scrW   = opts.screenW;
    if (opts.screenH !== undefined) this._scrH   = opts.screenH;
    this._resize();
  }

  /** Clear the screen. */
  clear() {
    const ctx = this.ctx;
    ctx.fillStyle = this.colorBg;
    ctx.fillRect(0, 0, this.canvas.width, this.canvas.height);
  }

  /**
   * Draw a rendered frame onto the OLED canvas.
   *
   * @param {Uint8Array} pixels  - flat pixel array (0=off,255=on), width×height
   * @param {number}     width   - frame width in pixels
   * @param {number}     height  - frame height in pixels
   * @param {number}     [ox=0]  - x offset on screen
   * @param {number}     [oy=0]  - y offset on screen
   * @param {boolean}    [clipping=false]
   */
  drawPixels(pixels, width, height, ox = 0, oy = 0, clipping = false) {
    const ctx  = this.ctx;
    const zoom = this.zoom;

    for (let row = 0; row < height; row++) {
      const sy = oy + row;
      if (clipping && (sy < 0 || sy >= this._scrH)) continue;
      for (let col = 0; col < width; col++) {
        const sx = ox + col;
        if (clipping && (sx < 0 || sx >= this._scrW)) continue;
        const on = pixels[row * width + col] !== 0;
        ctx.fillStyle = on ? this.colorOn : this.colorBg;
        ctx.fillRect(sx * zoom, sy * zoom, zoom, zoom);
      }
    }

    if (this.grid) this._drawGrid();
    if (this.bbox) this._drawBbox(ox, oy, width, height);
  }

  _drawGrid() {
    const ctx  = this.ctx;
    const zoom = this.zoom;
    if (zoom < 4) return; // grid is only useful at higher zoom levels
    ctx.strokeStyle = 'rgba(255,255,255,0.08)';
    ctx.lineWidth   = 0.5;
    for (let x = 0; x <= this._scrW; x++) {
      ctx.beginPath();
      ctx.moveTo(x * zoom, 0);
      ctx.lineTo(x * zoom, this._scrH * zoom);
      ctx.stroke();
    }
    for (let y = 0; y <= this._scrH; y++) {
      ctx.beginPath();
      ctx.moveTo(0, y * zoom);
      ctx.lineTo(this._scrW * zoom, y * zoom);
      ctx.stroke();
    }
  }

  _drawBbox(ox, oy, w, h) {
    const ctx  = this.ctx;
    const zoom = this.zoom;
    ctx.strokeStyle = 'rgba(255,200,0,0.7)';
    ctx.lineWidth   = 1;
    ctx.strokeRect(ox * zoom + 0.5, oy * zoom + 0.5, w * zoom - 1, h * zoom - 1);
  }
}

// ===========================================================================
// Asset Viewer
// ===========================================================================

const AssetViewer = (() => {
  // State
  let _assetList   = [];
  let _currentAsset = null;  // full asset detail from /api/assets/:name
  let _frameIndex  = 0;
  let _playing     = false;
  let _playTimer   = null;
  let _oled        = null;

  // DOM refs
  const $assetSelect  = document.getElementById('assetSelect');
  const $formatSelect = document.getElementById('formatSelect');
  const $frameInput   = document.getElementById('frameInput');
  const $frameTotal   = document.getElementById('frameTotal');
  const $btnPrev      = document.getElementById('btnPrevFrame');
  const $btnPlay      = document.getElementById('btnPlayPause');
  const $btnNext      = document.getElementById('btnNextFrame');
  const $fpsRange     = document.getElementById('fpsRange');
  const $fpsValue     = document.getElementById('fpsValue');
  const $zoomRange    = document.getElementById('zoomRange');
  const $zoomValue    = document.getElementById('zoomValue');
  const $chkGrid      = document.getElementById('chkGrid');
  const $chkInvert    = document.getElementById('chkInvert');
  const $chkMirrorH   = document.getElementById('chkMirrorH');
  const $chkMirrorV   = document.getElementById('chkMirrorV');
  const $rotSelect    = document.getElementById('rotationSelect');
  const $chkBbox      = document.getElementById('chkBoundingBox');
  const $colorOn      = document.getElementById('colorOn');
  const $colorBg      = document.getElementById('colorBg');
  const $infoContent  = document.getElementById('assetInfoContent');
  const $warnings     = document.getElementById('assetWarnings');
  const $canvas       = document.getElementById('assetCanvas');

  function init() {
    _oled = new OLEDCanvas($canvas, { zoom: 4 });
    _oled.clear();

    $assetSelect.addEventListener('change', onAssetChange);
    $formatSelect.addEventListener('change', render);
    $frameInput.addEventListener('change',  () => { _frameIndex = parseInt($frameInput.value) || 0; render(); });
    $btnPrev.addEventListener('click',      prevFrame);
    $btnPlay.addEventListener('click',      togglePlay);
    $btnNext.addEventListener('click',      nextFrame);
    $fpsRange.addEventListener('input',     () => { $fpsValue.textContent = $fpsRange.value; if (_playing) restartPlay(); });
    $zoomRange.addEventListener('input',    () => { $zoomValue.textContent = $zoomRange.value + '×'; render(); });
    $chkGrid.addEventListener('change',     render);
    $chkInvert.addEventListener('change',   render);
    $chkMirrorH.addEventListener('change',  render);
    $chkMirrorV.addEventListener('change',  render);
    $rotSelect.addEventListener('change',   render);
    $chkBbox.addEventListener('change',     render);
    $colorOn.addEventListener('input',      render);
    $colorBg.addEventListener('input',      render);
  }

  function loadAssetList(list) {
    _assetList = list;
    const prev = $assetSelect.value;
    $assetSelect.innerHTML = '<option value="">— none —</option>';
    for (const a of list) {
      const opt = document.createElement('option');
      opt.value       = a.name;
      opt.textContent = `${a.name}${a.loaded ? '' : ' ⚠'}`;
      $assetSelect.appendChild(opt);
    }
    if (prev && list.find(a => a.name === prev)) $assetSelect.value = prev;
    // Also update panel tab select
    PanelPreview.loadAssetList(list);
  }

  async function onAssetChange() {
    stopPlay();
    const name = $assetSelect.value;
    if (!name) { _currentAsset = null; renderInfo(); render(); return; }
    try {
      _currentAsset = await API.getAsset(name);
      _frameIndex   = 0;
      $frameInput.value = 0;
      // Default the format select to match the declared format
      if (_currentAsset.format) $formatSelect.value = _currentAsset.format;
    } catch (e) {
      _currentAsset = null;
      showError(e.message);
    }
    renderInfo();
    render();
  }

  function render() {
    if (!_currentAsset || !_currentAsset.bytesBase64) {
      _oled.setOptions({
        zoom:    parseInt($zoomRange.value),
        colorOn: $colorOn.value,
        colorBg: $colorBg.value,
        grid:    $chkGrid.checked,
        bbox:    $chkBbox.checked,
      });
      _oled.clear();
      return;
    }

    const a        = _currentAsset;
    const format   = $formatSelect.value;
    const zoom     = parseInt($zoomRange.value);
    const rotation = parseInt($rotSelect.value);

    // Decode all bytes
    const allBytes = BitmapDecoder.b64ToBytes(a.bytesBase64);

    // Extract the right frame
    const frames     = a.frames || 1;
    const frameBytes = a.frameStride
      ? a.frameStride
      : Math.floor(allBytes.length / frames);

    const fi     = Math.max(0, Math.min(_frameIndex, frames - 1));
    const start  = (a.offset ?? 0) + fi * frameBytes;
    const end    = start + frameBytes;
    const chunk  = allBytes.slice(start, Math.min(end, allBytes.length));

    const result = BitmapDecoder.renderBitmap(chunk, a.width, a.height, format, {
      invertColor: $chkInvert.checked,
      mirrorH:     $chkMirrorH.checked,
      mirrorV:     $chkMirrorV.checked,
      rotation,
    });

    _oled.setOptions({
      zoom,
      colorOn:  $colorOn.value,
      colorBg:  $colorBg.value,
      grid:     $chkGrid.checked,
      bbox:     $chkBbox.checked,
      screenW:  result.width,
      screenH:  result.height,
    });
    _oled.clear();
    _oled.drawPixels(result.pixels, result.width, result.height, 0, 0, false);

    showWarnings([...result.warnings, ...(a.warnings ?? []), ...(a.errors ?? [])]);
  }

  function renderInfo() {
    const a = _currentAsset;
    if (!a) { $infoContent.innerHTML = '<em>No asset selected</em>'; return; }
    const expected = computeExpected(a);
    const actual   = a.pixelByteCount ?? 0;
    const mismatch = expected !== null && actual !== expected;
    $infoContent.innerHTML = kv([
      ['Name',          a.name],
      ['Symbol',        a.symbol],
      ['Source',        shortPath(a.source)],
      ['Format',        a.format],
      ['Declared W×H', `${a.width}×${a.height}`],
      ['Frames',        a.frames],
      ['Inferred W',   a.inferredWidth  ?? '—'],
      ['Inferred H',   a.inferredHeight ?? '—'],
      ['Raw bytes',    a.rawByteCount],
      ['Pixel bytes',  actual + (mismatch ? ` ⚠ expected ${expected}` : '')],
      ['Loaded',        a.loaded ? 'yes' : 'NO'],
    ], mismatch ? ['Pixel bytes'] : []);

    $frameTotal.textContent = `/ ${(a.frames ?? 1) - 1}`;
    $frameInput.max = Math.max(0, (a.frames ?? 1) - 1);
  }

  function kv(pairs, warnKeys = []) {
    return pairs.map(([k, v]) =>
      `<div class="kv"><span class="info-key">${k}:</span>` +
      `<span class="info-val${warnKeys.includes(k) ? ' warn' : ''}">${v}</span></div>`
    ).join('');
  }

  function computeExpected(a) {
    if (!a.width || !a.height) return null;
    const stride = Math.ceil(a.width / 8);
    const frames = a.frames || 1;
    return stride * a.height * frames;
  }

  function shortPath(p) {
    if (!p) return '—';
    const parts = p.replace(/\\/g, '/').split('/');
    return parts.length > 3 ? '…/' + parts.slice(-2).join('/') : p;
  }

  function showWarnings(list) {
    $warnings.innerHTML = list.map(w => {
      const cls = w.toLowerCase().includes('error') ? 'error-item' : 'warn-item';
      return `<div class="${cls}">${escapeHtml(w)}</div>`;
    }).join('');
  }

  function showError(msg) {
    $warnings.innerHTML = `<div class="error-item">${escapeHtml(msg)}</div>`;
  }

  function prevFrame() { _frameIndex = Math.max(0, _frameIndex - 1); sync(); render(); }
  function nextFrame() {
    const max = (_currentAsset?.frames ?? 1) - 1;
    _frameIndex = Math.min(max, _frameIndex + 1);
    sync(); render();
  }
  function sync() { $frameInput.value = _frameIndex; }

  function togglePlay() {
    if (_playing) stopPlay(); else startPlay();
  }
  function startPlay() {
    if (!_currentAsset || (_currentAsset.frames ?? 1) <= 1) return;
    _playing = true;
    $btnPlay.textContent = '⏸ Pause';
    restartPlay();
  }
  function stopPlay() {
    _playing = false;
    $btnPlay.textContent = '▶ Play';
    if (_playTimer) { clearInterval(_playTimer); _playTimer = null; }
  }
  function restartPlay() {
    if (_playTimer) clearInterval(_playTimer);
    const fps = parseInt($fpsRange.value) || 8;
    _playTimer = setInterval(() => {
      const max = (_currentAsset?.frames ?? 1);
      _frameIndex = (_frameIndex + 1) % max;
      sync();
      render();
    }, 1000 / fps);
  }

  return { init, loadAssetList };
})();

// ===========================================================================
// Virtual Assets — client-side synthetic assets rendered by JS
// ===========================================================================

const VirtualAssets = (() => {
  function drawBatteryGauge(w, h, state) {
    const offscreen  = document.createElement('canvas');
    offscreen.width  = w;
    offscreen.height = h;
    const ctx = offscreen.getContext('2d');

    ctx.fillStyle = '#000';
    ctx.fillRect(0, 0, w, h);

    const batt     = state?.battery ?? { left: 0, right: 0, charging: false };
    const leftPct  = Math.max(0, Math.min(100, batt.left));
    const rightPct = Math.max(0, Math.min(100, batt.right));
    const charging = batt.charging;

    const fontSize = Math.max(7, Math.floor(h * 0.65));
    ctx.font         = `bold ${fontSize}px monospace`;
    ctx.textBaseline = 'top';
    ctx.fillStyle    = '#fff';

    const half = Math.floor(w / 2) - 2;
    const pad  = 2;
    const barH = Math.max(2, Math.floor(h * 0.22));
    const barY = h - barH - 1;

    // Left half
    const lText = `L:${leftPct}%${charging ? '\u26A1' : ''}`;
    ctx.fillText(lText, pad, 0);
    const lBarW = Math.round((half - pad) * leftPct / 100);
    ctx.fillRect(pad, barY, lBarW, barH);
    ctx.strokeStyle = '#888';
    ctx.lineWidth   = 0.5;
    ctx.strokeRect(pad - 0.5, barY - 0.5, half - pad + 1, barH + 1);

    // Right half
    const rText = `R:${rightPct}%`;
    ctx.fillStyle = '#fff';
    ctx.fillText(rText, half + pad + 2, 0);
    const rBarW = Math.round((half - pad) * rightPct / 100);
    ctx.fillRect(half + pad + 2, barY, rBarW, barH);
    ctx.strokeStyle = '#888';
    ctx.strokeRect(half + pad + 2 - 0.5, barY - 0.5, half - pad + 1, barH + 1);

    // Convert to 1-bpp pixel array
    const imgData = ctx.getImageData(0, 0, w, h);
    const pixels  = new Uint8Array(w * h);
    for (let i = 0; i < w * h; i++) {
      pixels[i] = imgData.data[i * 4] > 64 ? 255 : 0;
    }
    return pixels;
  }

  const _assets = {
    battery_gauge: {
      name:    'battery_gauge',
      width:   128,
      height:  24,
      frames:  1,
      format:  'virtual',
      render:  drawBatteryGauge,
    },
  };

  return {
    list:      () => Object.values(_assets),
    get:       name => _assets[name] ?? null,
    isVirtual: name => name in _assets,
  };
})();

// ===========================================================================
// Panel Preview
// ===========================================================================

const PanelPreview = (() => {
  const SIDES = ['left', 'right'];

  // Cached DOM refs per half (resolved at module eval time, DOM is ready)
  const _dom = {
    left: {
      canvas:     document.getElementById('panelCanvasLeft'),
      assetSel:   document.getElementById('panelAssetSelectLeft'),
      xInput:     document.getElementById('panelXLeft'),
      yInput:     document.getElementById('panelYLeft'),
      anchor:     document.getElementById('panelAnchorLeft'),
      clearBef:   document.getElementById('panelClearLeft'),
      clipping:   document.getElementById('panelClipLeft'),
      invert:     document.getElementById('panelInvertLeft'),
      stage:      document.getElementById('stageLeft'),
      orientBtns: document.getElementById('orientBtnsLeft'),
    },
    right: {
      canvas:     document.getElementById('panelCanvasRight'),
      assetSel:   document.getElementById('panelAssetSelectRight'),
      xInput:     document.getElementById('panelXRight'),
      yInput:     document.getElementById('panelYRight'),
      anchor:     document.getElementById('panelAnchorRight'),
      clearBef:   document.getElementById('panelClearRight'),
      clipping:   document.getElementById('panelClipRight'),
      invert:     document.getElementById('panelInvertRight'),
      stage:      document.getElementById('stageRight'),
      orientBtns: document.getElementById('orientBtnsRight'),
    },
  };

  const $zoom    = document.getElementById('panelZoom');
  const $zoomVal = document.getElementById('panelZoomValue');
  const $grid    = document.getElementById('panelGrid');

  // Per-half runtime state
  const _half = {
    left:  { oled: null, asset: null, frameIndex: 0, orientation: 'landscape' },
    right: { oled: null, asset: null, frameIndex: 0, orientation: 'landscape' },
  };

  let _lastState = null;

  function init() {
    const zoom = parseInt($zoom.value);
    for (const side of SIDES) {
      _half[side].oled = new OLEDCanvas(_dom[side].canvas, {
        screenW: 128, screenH: 64, zoom, colorOn: '#00ff88', colorBg: '#000',
      });
      _half[side].oled.clear();

      const d = _dom[side];
      d.assetSel.addEventListener('change', () => onAssetChange(side));
      [d.xInput, d.yInput, d.anchor, d.clearBef, d.clipping, d.invert]
        .forEach(el => el.addEventListener('change', () => renderHalf(side, _lastState)));

      for (const btn of d.orientBtns.querySelectorAll('.orient-btn')) {
        btn.addEventListener('click', () => setOrientation(side, btn.dataset.orient));
      }
    }

    $zoom.addEventListener('input', () => {
      $zoomVal.textContent = $zoom.value + '\xd7';
      for (const side of SIDES) {
        _half[side].oled.setOptions({ zoom: parseInt($zoom.value) });
        // Re-apply orientation so stage dimensions update for new zoom
        setOrientation(side, _half[side].orientation, /* noRender= */ true);
      }
      renderAll(_lastState);
    });
    $grid.addEventListener('change', () => renderAll(_lastState));
  }

  function setOrientation(side, orient, noRender = false) {
    _half[side].orientation = orient;
    const stageEl = _dom[side].stage;
    const wrapEl  = stageEl.querySelector('.oled-wrap');
    const zoom    = parseInt($zoom.value);
    const cw      = 128 * zoom;  // canvas width in px
    const ch      =  64 * zoom;  // canvas height in px

    stageEl.classList.remove('portrait-cw', 'portrait-ccw');
    if (orient === 'cw' || orient === 'ccw') {
      stageEl.classList.add(`portrait-${orient}`);
      // Stage takes the portrait footprint; canvas is centered inside and rotated
      stageEl.style.width  = ch + 'px';
      stageEl.style.height = cw + 'px';
      wrapEl.style.left    = ((ch - cw) / 2) + 'px';
      wrapEl.style.top     = ((cw - ch) / 2) + 'px';
    } else {
      stageEl.style.width  = '';
      stageEl.style.height = '';
      wrapEl.style.left    = '';
      wrapEl.style.top     = '';
    }

    for (const btn of _dom[side].orientBtns.querySelectorAll('.orient-btn')) {
      btn.classList.toggle('active', btn.dataset.orient === orient);
    }
    if (!noRender) renderHalf(side, _lastState);
  }

  function loadAssetList(list) {
    for (const side of SIDES) {
      const sel  = _dom[side].assetSel;
      const prev = sel.value;
      sel.innerHTML = '<option value="">— none —</option>';
      for (const a of list) {
        const opt = document.createElement('option');
        opt.value       = a.name;
        opt.textContent = a.name;
        sel.appendChild(opt);
      }
      for (const va of VirtualAssets.list()) {
        const opt = document.createElement('option');
        opt.value       = va.name;
        opt.textContent = '\u26a1 ' + va.name;
        sel.appendChild(opt);
      }
      if (prev && (list.find(a => a.name === prev) || VirtualAssets.isVirtual(prev))) {
        sel.value = prev;
      }
    }
  }

  async function onAssetChange(side) {
    const name = _dom[side].assetSel.value;
    if (!name) { _half[side].asset = null; renderHalf(side, _lastState); return; }
    if (VirtualAssets.isVirtual(name)) {
      _half[side].asset = VirtualAssets.get(name);
      renderHalf(side, _lastState);
      return;
    }
    try {
      _half[side].asset = await API.getAsset(name);
    } catch (e) {
      _half[side].asset = null;
    }
    renderHalf(side, _lastState);
  }

  function renderHalf(side, kbState) {
    const h   = _half[side];
    const d   = _dom[side];
    const zoom = parseInt($zoom.value);
    h.oled.setOptions({ zoom, grid: $grid.checked, screenW: 128, screenH: 64 });

    if (d.clearBef.checked || !h.asset) { h.oled.clear(); }
    if (!h.asset) return;

    const a = h.asset;
    let pixels, pw, ph;

    if (a.format === 'virtual') {
      pixels = a.render(a.width, a.height, kbState);
      pw = a.width;
      ph = a.height;
    } else {
      if (!a.bytesBase64) return;
      const allBytes = BitmapDecoder.b64ToBytes(a.bytesBase64);
      const frames   = a.frames || 1;
      const fBytes   = Math.floor(allBytes.length / frames);
      const fi       = Math.max(0, Math.min(h.frameIndex, frames - 1));
      const chunk    = allBytes.slice(fi * fBytes, fi * fBytes + fBytes);
      const decoded  = BitmapDecoder.renderBitmap(chunk, a.width, a.height, a.format, {
        invertColor: d.invert.checked,
      });
      pixels = decoded.pixels;
      pw     = decoded.width;
      ph     = decoded.height;
    }

    let ox = parseInt(d.xInput.value) || 0;
    let oy = parseInt(d.yInput.value) || 0;
    const anchor = d.anchor.value;
    if (anchor === 'top-right')    { ox -= pw; }
    if (anchor === 'center')       { ox -= pw >> 1; oy -= ph >> 1; }
    if (anchor === 'bottom-left')  { oy -= ph; }
    if (anchor === 'bottom-right') { ox -= pw; oy -= ph; }

    h.oled.drawPixels(pixels, pw, ph, ox, oy, d.clipping.checked);
  }

  function renderAll(kbState) {
    _lastState = kbState;
    for (const side of SIDES) renderHalf(side, kbState);
  }

  function setFrame(idx) {
    for (const side of SIDES) _half[side].frameIndex = idx;
    renderAll(_lastState);
  }

  return { init, loadAssetList, renderAll, setFrame };
})();

// ===========================================================================
// State Panel
// ===========================================================================

const StatePanel = (() => {
  const LAYERS = [
    { id: 0, key: 'WINDOWS',    name: 'Windows'   },
    { id: 1, key: 'WINDOWS_FN', name: 'Win Fn'    },
    { id: 2, key: 'MAC',        name: 'Mac'        },
    { id: 3, key: 'MAC_FN',     name: 'Mac Fn'     },
    { id: 4, key: 'GALLIUM',    name: 'Gallium'    },
    { id: 5, key: 'GALLIUM_FN', name: 'Gallium Fn' },
    { id: 6, key: 'UTILS',      name: 'Utils'      },
  ];

  let _state           = null;
  let _keyCapture      = false;

  // DOM refs
  const $battLeft    = document.getElementById('battLeft');
  const $battRight   = document.getElementById('battRight');
  const $battLeftV   = document.getElementById('battLeftVal');
  const $battRightV  = document.getElementById('battRightVal');
  const $chkCharging = document.getElementById('chkCharging');
  const $outputSel   = document.getElementById('outputSelect');
  const $chkUsb      = document.getElementById('chkUsb');
  const $chkBle      = document.getElementById('chkBle');
  const $bleProfile  = document.getElementById('bleProfile');
  const $chkAdv      = document.getElementById('chkAdv');
  const $chkPairing  = document.getElementById('chkPairing');
  const $chkSplitL   = document.getElementById('chkSplitL');
  const $chkSplitR   = document.getElementById('chkSplitR');
  const $linkQ       = document.getElementById('linkQuality');
  const $chkIdle     = document.getElementById('chkIdle');
  const $chkSleep    = document.getElementById('chkSleep');
  const $chkSoftOff  = document.getElementById('chkSoftOff');
  const $layerBtns   = document.getElementById('layerButtons');
  const $pressedKeys = document.getElementById('pressedKeys');
  const $stateJson   = document.getElementById('stateJson');
  const $tapInput    = document.getElementById('tapKeyInput');
  const $btnTap      = document.getElementById('btnTapKey');
  const $btnCapture  = document.getElementById('btnKeyCapture');
  const $btnClear    = document.getElementById('btnClearKeys');
  const $btnReset    = document.getElementById('btnResetState');
  const $wpmRange    = document.getElementById('wpmRange');
  const $wpmVal      = document.getElementById('wpmVal');
  const $actRange    = document.getElementById('actRange');
  const $actVal      = document.getElementById('actVal');

  function init() {
    // Build layer buttons
    for (const layer of LAYERS) {
      const btn = document.createElement('button');
      btn.className     = 'layer-btn';
      btn.dataset.layer = layer.id;
      btn.textContent   = layer.name;
      btn.addEventListener('click', () => sendPatch({ keyboard: { activeLayer: layer.id } }));
      $layerBtns.appendChild(btn);
    }

    // Battery
    $battLeft.addEventListener('input',  () => { $battLeftV.textContent  = $battLeft.value;  sendPatch({ battery: { left:  parseInt($battLeft.value)  } }); });
    $battRight.addEventListener('input', () => { $battRightV.textContent = $battRight.value; sendPatch({ battery: { right: parseInt($battRight.value) } }); });
    $chkCharging.addEventListener('change', () => sendPatch({ battery:    { charging:     $chkCharging.checked } }));

    // Connection
    $outputSel.addEventListener('change',  () => sendPatch({ connection: { output:       $outputSel.value    } }));
    $chkUsb.addEventListener('change',     () => sendPatch({ connection: { usbConnected: $chkUsb.checked     } }));
    $chkBle.addEventListener('change',     () => sendPatch({ connection: { bleConnected: $chkBle.checked     } }));
    $bleProfile.addEventListener('change', () => sendPatch({ connection: { bleProfile:   parseInt($bleProfile.value) } }));
    $chkAdv.addEventListener('change',     () => sendPatch({ connection: { advertising:  $chkAdv.checked     } }));
    $chkPairing.addEventListener('change', () => sendPatch({ connection: { pairing:      $chkPairing.checked } }));

    // Split
    $chkSplitL.addEventListener('change', () => sendPatch({ split: { leftConnected:  $chkSplitL.checked } }));
    $chkSplitR.addEventListener('change', () => sendPatch({ split: { rightConnected: $chkSplitR.checked } }));
    $linkQ.addEventListener('change',     () => sendPatch({ split: { linkQuality:    $linkQ.value        } }));

    // Power
    $chkIdle.addEventListener('change',    () => sendPatch({ power: { idle:    $chkIdle.checked    } }));
    $chkSleep.addEventListener('change',   () => sendPatch({ power: { sleep:   $chkSleep.checked   } }));
    $chkSoftOff.addEventListener('change', () => sendPatch({ power: { softOff: $chkSoftOff.checked } }));

    // Mods
    for (const el of document.querySelectorAll('[data-mod]')) {
      el.addEventListener('change', () => {
        sendPatch({ keyboard: { mods: { [el.dataset.mod]: el.checked } } });
      });
    }

    // Locks
    for (const el of document.querySelectorAll('[data-lock]')) {
      el.addEventListener('change', () => {
        sendPatch({ keyboard: { locks: { [el.dataset.lock]: el.checked } } });
      });
    }

    // WPM / activity
    $wpmRange.addEventListener('input', () => { $wpmVal.textContent = $wpmRange.value; sendPatch({ keyboard: { wpm:      parseInt($wpmRange.value)  } }); });
    $actRange.addEventListener('input', () => { $actVal.textContent = $actRange.value; sendPatch({ keyboard: { activity: parseInt($actRange.value)  } }); });

    // Key actions
    $btnTap.addEventListener('click', async () => {
      const key = $tapInput.value.trim();
      if (!key) return;
      try {
        const r = await API.postKeyEvent({ type: 'tap', key });
        applyKeyboardFromState(r.keyboard);
        $tapInput.value = '';
      } catch (e) { console.warn(e); }
    });

    $tapInput.addEventListener('keydown', (e) => {
      if (e.key === 'Enter') $btnTap.click();
    });

    $btnClear.addEventListener('click', async () => {
      try {
        const r = await API.postKeyEvent({ type: 'clear' });
        applyKeyboardFromState(r.keyboard);
      } catch (e) { console.warn(e); }
    });

    $btnReset.addEventListener('click', async () => {
      try {
        const r = await API.resetState();
        applyState(r.state);
      } catch (e) { console.warn(e); }
    });

    $btnCapture.addEventListener('click', () => {
      _keyCapture = !_keyCapture;
      $btnCapture.classList.toggle('active', _keyCapture);
    });

    document.addEventListener('keydown', (e) => {
      if (!_keyCapture) return;
      e.preventDefault();
      API.postKeyEvent({ type: 'key_down', key: e.key }).then(r => applyKeyboardFromState(r.keyboard));
    });
    document.addEventListener('keyup', (e) => {
      if (!_keyCapture) return;
      API.postKeyEvent({ type: 'key_up', key: e.key }).then(r => applyKeyboardFromState(r.keyboard));
    });
  }

  async function sendPatch(partial) {
    try {
      const r = await API.patchState(partial);
      _state = r.state;
      syncUI();
      PanelPreview.renderAll(_state);
    } catch (e) { console.warn('patchState error:', e.message); }
  }

  async function refresh() {
    try {
      _state = await API.getState();
      applyState(_state);
    } catch (e) { console.warn('getState error:', e.message); }
  }

  function applyState(state) {
    _state = state;
    syncUI();
    PanelPreview.renderAll(state);
  }

  function applyKeyboardFromState(kb) {
    if (!_state) return;
    _state.keyboard = kb;
    syncUI();
  }

  function syncUI() {
    if (!_state) return;
    const s = _state;

    // Battery
    $battLeft.value  = s.battery.left;
    $battLeftV.textContent  = s.battery.left;
    $battRight.value = s.battery.right;
    $battRightV.textContent = s.battery.right;
    $chkCharging.checked    = s.battery.charging;

    // Connection
    $outputSel.value     = s.connection.output;
    $chkUsb.checked      = s.connection.usbConnected;
    $chkBle.checked      = s.connection.bleConnected;
    $bleProfile.value    = s.connection.bleProfile;
    $chkAdv.checked      = s.connection.advertising;
    $chkPairing.checked  = s.connection.pairing;

    // Split
    $chkSplitL.checked   = s.split.leftConnected;
    $chkSplitR.checked   = s.split.rightConnected;
    $linkQ.value         = s.split.linkQuality;

    // Power
    $chkIdle.checked    = s.power.idle;
    $chkSleep.checked   = s.power.sleep;
    $chkSoftOff.checked = s.power.softOff;

    // Layer buttons
    for (const btn of $layerBtns.querySelectorAll('.layer-btn')) {
      btn.classList.toggle('active', parseInt(btn.dataset.layer) === s.keyboard.activeLayer);
    }

    // Mods
    for (const el of document.querySelectorAll('[data-mod]')) {
      el.checked = s.keyboard.mods[el.dataset.mod] ?? false;
    }

    // Locks
    for (const el of document.querySelectorAll('[data-lock]')) {
      el.checked = s.keyboard.locks[el.dataset.lock] ?? false;
    }

    // Pressed keys
    if (s.keyboard.pressedKeys.length === 0) {
      $pressedKeys.innerHTML = '<em>No keys pressed</em>';
    } else {
      $pressedKeys.innerHTML = s.keyboard.pressedKeys
        .map(k => `<span class="key-chip">${escapeHtml(k)}</span>`)
        .join('');
    }

    // WPM / activity
    $wpmRange.value = s.keyboard.wpm;
    $wpmVal.textContent = s.keyboard.wpm;
    $actRange.value = s.keyboard.activity;
    $actVal.textContent = s.keyboard.activity;

    // State JSON
    $stateJson.textContent = JSON.stringify(s, null, 2);
  }

  return { init, refresh, applyState };
})();

// ===========================================================================
// Tab navigation
// ===========================================================================

function initTabs() {
  for (const btn of document.querySelectorAll('.tab-btn')) {
    btn.addEventListener('click', () => {
      const tab = btn.dataset.tab;
      document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
      document.querySelectorAll('.tab-pane').forEach(p => p.classList.remove('active'));
      btn.classList.add('active');
      document.getElementById(`tab-${tab}`).classList.add('active');
    });
  }
}

// ===========================================================================
// Server status + reload
// ===========================================================================

const $statusDot   = document.getElementById('statusDot');
const $statusLabel = document.getElementById('statusLabel');

function setStatus(ok) {
  $statusDot.className  = 'status-dot ' + (ok ? 'ok' : 'error');
  $statusLabel.textContent = ok ? 'connected' : 'disconnected';
}

document.getElementById('btnReloadAssets').addEventListener('click', async () => {
  try {
    const r = await API.reloadAssets();
    if (r.assets) AssetViewer.loadAssetList(r.assets);
    setStatus(true);
  } catch (e) {
    setStatus(false);
    console.error(e);
  }
});

// ===========================================================================
// Utility
// ===========================================================================

function escapeHtml(s) {
  return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');
}

// ===========================================================================
// Bootstrap
// ===========================================================================

async function main() {
  initTabs();
  AssetViewer.init();
  PanelPreview.init();
  StatePanel.init();

  try {
    const [assetsResp, state] = await Promise.all([
      API.getAssets(),
      API.getState(),
    ]);
    setStatus(true);

    if (assetsResp.assets) AssetViewer.loadAssetList(assetsResp.assets);
    StatePanel.applyState(state);

    // Log any top-level errors
    if (assetsResp.errors?.length) {
      console.warn('[assets] errors:', assetsResp.errors);
    }
  } catch (e) {
    setStatus(false);
    console.error('Init error:', e.message);
  }
}

main();
