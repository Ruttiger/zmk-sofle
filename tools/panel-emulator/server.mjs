/**
 * server.mjs
 *
 * Local HTTP server for the OLED Panel Emulator.
 * Serves static files from public/ and exposes a REST API.
 *
 * Usage:
 *   node server.mjs          # listens on http://localhost:5173
 *   PORT=3000 node server.mjs
 *
 * API routes:
 *   GET  /api/assets            — list all assets with metadata
 *   GET  /api/assets/:name      — detail for one asset (+ base64 pixel bytes)
 *   GET  /api/state             — current keyboard state
 *   PATCH /api/state            — partial state update
 *   POST /api/key-event         — process key event
 *   POST /api/reload-assets     — reload assets.json and source files
 *   POST /api/reset             — reset state to defaults
 */

import http  from 'node:http';
import fs    from 'node:fs';
import fsp   from 'node:fs/promises';
import path  from 'node:path';
import { fileURLToPath } from 'node:url';

import { loadAssets, reloadAssets } from './src/asset-loader.mjs';
import { getState, patchState, resetState, processKeyEvent } from './src/state.mjs';

// ---------------------------------------------------------------------------
// Paths
// ---------------------------------------------------------------------------

const __dirname   = path.dirname(fileURLToPath(import.meta.url));
const PUBLIC_DIR  = path.join(__dirname, 'public');
const ASSETS_JSON = path.join(__dirname, 'assets.json');
const PORT        = parseInt(process.env.PORT ?? '5173', 10);

// ---------------------------------------------------------------------------
// MIME types
// ---------------------------------------------------------------------------

const MIME = {
  '.html': 'text/html; charset=utf-8',
  '.css':  'text/css; charset=utf-8',
  '.js':   'application/javascript; charset=utf-8',
  '.mjs':  'application/javascript; charset=utf-8',
  '.json': 'application/json; charset=utf-8',
  '.ico':  'image/x-icon',
  '.png':  'image/png',
  '.svg':  'image/svg+xml',
};

// ---------------------------------------------------------------------------
// Asset store (loaded at startup, refreshed on POST /api/reload-assets)
// ---------------------------------------------------------------------------

let assetStore = { assets: new Map(), screen: { width: 128, height: 64 }, errors: [], warnings: [] };

async function initAssets() {
  try {
    assetStore = await loadAssets(ASSETS_JSON);
    const total    = assetStore.assets.size;
    const loaded   = [...assetStore.assets.values()].filter(a => a.loaded).length;
    const errCount = [...assetStore.assets.values()].reduce((n, a) => n + a.errors.length, 0);
    console.log(`[assets] Loaded ${loaded}/${total} assets. Errors: ${errCount}. Top-level errors: ${assetStore.errors.length}`);
    if (assetStore.errors.length)   assetStore.errors.forEach(e   => console.warn('[assets] ERROR:', e));
    if (assetStore.warnings.length) assetStore.warnings.forEach(w => console.warn('[assets] WARN:', w));
  } catch (err) {
    console.error('[assets] Unexpected error during init:', err.message);
  }
}

// ---------------------------------------------------------------------------
// HTTP helpers
// ---------------------------------------------------------------------------

function jsonResponse(res, status, body) {
  const payload = JSON.stringify(body, null, 2);
  res.writeHead(status, {
    'Content-Type':                'application/json; charset=utf-8',
    'Access-Control-Allow-Origin': '*',
    'Cache-Control':               'no-store',
  });
  res.end(payload);
}

async function readBody(req) {
  return new Promise((resolve, reject) => {
    const chunks = [];
    req.on('data', chunk => chunks.push(chunk));
    req.on('end',  () => {
      try { resolve(JSON.parse(Buffer.concat(chunks).toString('utf-8') || '{}')); }
      catch (e) { reject(new Error('Invalid JSON in request body')); }
    });
    req.on('error', reject);
  });
}

function serveStatic(res, filePath) {
  const ext  = path.extname(filePath).toLowerCase();
  const mime = MIME[ext] ?? 'application/octet-stream';

  fs.stat(filePath, (err, stat) => {
    if (err || !stat.isFile()) {
      // Fall through to index.html for SPA-style routing
      const index = path.join(PUBLIC_DIR, 'index.html');
      fs.readFile(index, (e2, data) => {
        if (e2) { res.writeHead(404); res.end('404 Not Found'); return; }
        res.writeHead(200, {
          'Content-Type':                MIME['.html'],
          'Access-Control-Allow-Origin': '*',
          'Cache-Control':               'no-store',
        });
        res.end(data);
      });
      return;
    }
    res.writeHead(200, {
      'Content-Type':                mime,
      'Content-Length':              stat.size,
      'Access-Control-Allow-Origin': '*',
      'Cache-Control':               'no-store',
    });
    fs.createReadStream(filePath).pipe(res);
  });
}

// ---------------------------------------------------------------------------
// Asset serialisation helpers
// ---------------------------------------------------------------------------

function assetMeta(entry) {
  return {
    name:           entry.name,
    source:         entry.source,
    symbol:         entry.symbol,
    width:          entry.width,
    height:         entry.height,
    frames:         entry.frames,
    format:         entry.format,
    stride:         entry.stride,
    frameStride:    entry.frameStride,
    offset:         entry.offset,
    notes:          entry.notes,
    rawByteCount:   entry.rawByteCount,
    pixelByteCount: entry.bytes ? entry.bytes.length : 0,
    inferredWidth:  entry.inferredWidth,
    inferredHeight: entry.inferredHeight,
    loaded:         entry.loaded,
    warnings:       entry.warnings,
    errors:         entry.errors,
  };
}

function assetDetail(entry) {
  const meta = assetMeta(entry);
  if (entry.bytes) {
    // Encode pixel bytes as base64 for client-side rendering
    meta.bytesBase64 = Buffer.from(entry.bytes).toString('base64');
  }
  if (entry.palette) {
    meta.paletteBase64 = Buffer.from(entry.palette).toString('base64');
  }
  return meta;
}

// ---------------------------------------------------------------------------
// Request router
// ---------------------------------------------------------------------------

async function handleRequest(req, res) {
  const { method, url } = req;
  const urlObj   = new URL(url, `http://localhost:${PORT}`);
  const pathname = urlObj.pathname;

  // CORS preflight
  if (method === 'OPTIONS') {
    res.writeHead(204, {
      'Access-Control-Allow-Origin':  '*',
      'Access-Control-Allow-Methods': 'GET, POST, PATCH, OPTIONS',
      'Access-Control-Allow-Headers': 'Content-Type',
    });
    res.end();
    return;
  }

  // ---- API routes ----

  // GET /api/assets
  if (method === 'GET' && pathname === '/api/assets') {
    const assets   = [...assetStore.assets.values()].map(assetMeta);
    const errCount = assets.reduce((n, a) => n + a.errors.length, 0);
    return jsonResponse(res, 200, {
      screen:   assetStore.screen,
      assets,
      errors:   assetStore.errors,
      warnings: assetStore.warnings,
      summary:  {
        total:  assets.length,
        loaded: assets.filter(a => a.loaded).length,
        errors: errCount,
      },
    });
  }

  // GET /api/assets/:name
  if (method === 'GET' && pathname.startsWith('/api/assets/')) {
    const name  = decodeURIComponent(pathname.slice('/api/assets/'.length));
    const entry = assetStore.assets.get(name);
    if (!entry) return jsonResponse(res, 404, { error: `Asset "${name}" not found.` });
    return jsonResponse(res, 200, assetDetail(entry));
  }

  // GET /api/state
  if (method === 'GET' && pathname === '/api/state') {
    return jsonResponse(res, 200, getState());
  }

  // PATCH /api/state
  if (method === 'PATCH' && pathname === '/api/state') {
    let body;
    try { body = await readBody(req); }
    catch (e) { return jsonResponse(res, 400, { error: e.message }); }
    const { applied, warnings } = patchState(body);
    return jsonResponse(res, 200, { state: getState(), applied, warnings });
  }

  // POST /api/key-event
  if (method === 'POST' && pathname === '/api/key-event') {
    let body;
    try { body = await readBody(req); }
    catch (e) { return jsonResponse(res, 400, { error: e.message }); }
    const { warnings } = processKeyEvent(body);
    return jsonResponse(res, 200, { keyboard: getState().keyboard, warnings });
  }

  // POST /api/reload-assets
  if (method === 'POST' && pathname === '/api/reload-assets') {
    await initAssets();
    const assets = [...assetStore.assets.values()].map(assetMeta);
    return jsonResponse(res, 200, {
      screen:   assetStore.screen,
      assets,
      errors:   assetStore.errors,
      warnings: assetStore.warnings,
    });
  }

  // POST /api/reset
  if (method === 'POST' && pathname === '/api/reset') {
    const state = resetState();
    return jsonResponse(res, 200, { state });
  }

  // ---- Static files ----
  if (method === 'GET') {
    let filePath;
    if (pathname === '/') {
      filePath = path.join(PUBLIC_DIR, 'index.html');
    } else {
      // Prevent directory traversal
      const rel = path.normalize(pathname).replace(/^(\.\.[/\\])+/, '');
      filePath = path.join(PUBLIC_DIR, rel);
    }
    return serveStatic(res, filePath);
  }

  // Catch-all
  return jsonResponse(res, 405, { error: 'Method not allowed' });
}

// ---------------------------------------------------------------------------
// Start server
// ---------------------------------------------------------------------------

(async () => {
  await initAssets();

  const server = http.createServer(async (req, res) => {
    try {
      await handleRequest(req, res);
    } catch (err) {
      console.error('[server] Unhandled error:', err);
      if (!res.headersSent) {
        jsonResponse(res, 500, { error: 'Internal server error', message: err.message });
      }
    }
  });

  server.listen(PORT, () => {
    console.log(`\n  OLED Panel Emulator`);
    console.log(`  → http://localhost:${PORT}\n`);
    console.log(`  API endpoints:`);
    console.log(`    GET  /api/assets`);
    console.log(`    GET  /api/assets/:name`);
    console.log(`    GET  /api/state`);
    console.log(`    PATCH /api/state`);
    console.log(`    POST /api/key-event`);
    console.log(`    POST /api/reload-assets`);
    console.log(`    POST /api/reset\n`);
  });
})();
