# OLED Panel Emulator — Sofle ZMK

Local web tool for previewing and debugging the OLED panel graphics used by the Sofle ZMK firmware, without compiling or flashing firmware.

## Quick start

```bash
cd tools/panel-emulator
npm install      # no production dependencies; only built-in Node.js modules used
npm start
```

Open **http://localhost:5173** in your browser.

To use a different port:

```bash
PORT=3000 npm start
```

Run tests:

```bash
npm test
```

---

## FASE 0 — Asset inventory

### Files found in this repo with graphic data

| File | Format | Symbols | Status |
|------|--------|---------|--------|
| `boards/shields/eyelash_sofle_animation/assets/placeholder.c` | LVGL `LV_IMG_CF_INDEXED_1BIT` 160×68 px | `placeholder_00_map`, `placeholder_01_map` | ✅ Supported (Phase 1) |
| `boards/shields/eyelash_sofle_animation/assets/placeholder.h` | LVGL declarations | `placeholder_00`, `placeholder_01`, `placeholder_images[]` | Declarations only, no pixel data |

### Widget controller files (no pixel data locally)

| File | Purpose |
|------|---------|
| `zmk-nice-oled-patches/boards/shields/nice_oled/widgets/animation.c` | Peripheral animation controller; declares cat_0–7, crystal_01–16, head, spaceman, pokemon, vim, vip_marcos via `LV_IMG_DECLARE()` |
| `zmk-nice-oled-patches/boards/shields/nice_oled/widgets/luna.c` | Central Luna (dog) animation controller; declares dog_sit1_90, dog_sit2_90, dog_walk1_90/2, dog_run1_90/2, dog_sneak1_90/2 |
| `zmk-nice-oled-patches/boards/shields/nice_oled/widgets/screen.c` | Central display rendering |
| `zmk-nice-oled-patches/boards/shields/nice_oled/widgets/screen_peripheral.c` | Right-half peripheral display rendering |
| `zmk-nice-oled-patches/boards/shields/nice_oled/widgets/sleep_status.c` | Sleep/idle art logic |

### External module assets (NOT available locally)

The following animation frames live in the external module `zmk-nice-oled`:
- **Cat** (right half, active): `cat_0` … `cat_7` — 8 frames
- **Luna/Dog** (left half, WPM-reactive): `dog_sit1_90`, `dog_sit2_90`, `dog_walk1_90`, `dog_walk2_90`, `dog_run1_90`, `dog_run2_90`, `dog_sneak1_90`, `dog_sneak2_90` — 8 frames (pre-rotated 90°)
- **Crystal** (fallback): `crystal_01` … `crystal_16` — 16 frames
- **Head**: `head_00` … `head_15` — 16 frames
- **Spaceman**: `spaceman_00` … `spaceman_19` — 20 frames
- **Pokemon**: `pokemon_00` … `pokemon_47` — 48 frames
- **Static**: `vim`, `vip_marcos`

To use external assets, see the section [Adding external zmk-nice-oled assets](#adding-external-zmk-nice-oled-assets) below.

---

## How to add an asset

### 1. Edit `assets.json`

```json
{
  "assets": [
    {
      "name":   "my_icon",
      "source": "../../path/to/icons.c",
      "symbol": "icon_battery",
      "width":  16,
      "height": 8,
      "frames": 1,
      "format": "lvgl-indexed-1bit"
    }
  ]
}
```

`source` is always **relative to `assets.json`** (i.e. relative to `tools/panel-emulator/`).

### 2. Reload without restart

```bash
curl -X POST http://localhost:5173/api/reload-assets
```

Or click **⟳ Reload assets** in the browser UI.

---

## assets.json reference

| Field | Required | Description |
|-------|----------|-------------|
| `name` | yes | Unique identifier shown in the UI |
| `source` | yes | Path to C source file, relative to `assets.json` |
| `symbol` | yes | Name of the C array (e.g. `my_icon_map`) |
| `width` | yes* | Pixel width. Required unless inferable from lv_img_dsc_t |
| `height` | yes* | Pixel height |
| `frames` | no | Number of animation frames (default: 1) |
| `format` | no | Bitmap format (default: `lvgl-indexed-1bit`) |
| `stride` | no | Bytes per row override |
| `frameStride` | no | Bytes per frame override |
| `offset` | no | Byte offset into the array |
| `notes` | no | Free-text annotation shown in UI |

---

## Supported bitmap formats

| Format | Description |
|--------|-------------|
| `lvgl-indexed-1bit` | LVGL `LV_IMG_CF_INDEXED_1BIT`: 8-byte palette prefix + 1bpp horizontal MSB. **Default.** |
| `mono-horizontal-msb` | 1bpp, row-major, MSB of first byte = leftmost pixel |
| `mono-horizontal-lsb` | 1bpp, row-major, LSB of first byte = leftmost pixel |
| `mono-vertical-pages-lsb` | OLED page-addressed (8px-tall pages), bit 0 = topmost pixel |
| `mono-vertical-pages-msb` | OLED page-addressed, bit 7 = topmost pixel |
| `mono-packed-rows` | Alias for `mono-horizontal-msb` |
| `mono-packed-columns` | Alias for `mono-vertical-pages-lsb` |

### Formats pending (not yet implemented)

| Format | Notes |
|--------|-------|
| `.xbm` files | X11 Bitmap format (C array with bit-reversed rows) |
| `.pbm` files | Portable Bitmap (binary or ASCII) |
| `.bmp` files | Windows Bitmap |
| Multi-symbol LVGL files | Single C file containing multiple `lv_img_dsc_t` structs |
| LVGL `LV_IMG_CF_TRUE_COLOR` | 16-bit or 32-bit colour |
| Run-length encoded | Not used by this firmware but common in ZMK community |

---

## Display hardware notes

- **Physical OLED**: SSD1306, 128×64, I2C, address `0x3C`
- **Firmware canvas**: 64×128 portrait mode (rotated before sending to hardware)
  - Left half: rotated 90° clockwise
  - Right half: rotated 270° clockwise
- **This emulator**: shows 128×64 by default (post-rotation view). Use the **Canvas rotation** control in the Asset Viewer to match firmware's pre-rotation internal view.

---

## Adding external zmk-nice-oled assets

The cat, dog/Luna, crystal, spaceman and pokemon assets are in the external module. To use them locally:

```bash
# From the repo root:
git clone -b panel_editions_ssd1306_fix \
  https://github.com/Ruttiger/zmk-nice-oled \
  ../zmk-nice-oled
```

Then add entries to `assets.json` pointing at the cloned path:

```json
{
  "name":   "cat_animation",
  "source": "../../../zmk-nice-oled/boards/shields/nice_oled/widgets/images/cat_0.c",
  "symbol": "cat_0_map",
  "width":  32,
  "height": 32,
  "frames": 8,
  "format": "lvgl-indexed-1bit",
  "notes":  "Right-half peripheral cat animation, 8 frames"
}
```

> **Note**: The exact dimensions and file paths in the external module are TBD until it is cloned.
> Inspect each `.c` file for the `lv_img_dsc_t` struct which declares `.header.w` and `.header.h`.

---

## Using the Asset Viewer

1. Select an asset from the dropdown.
2. The declared format is pre-selected; change it in the **Bitmap format** dropdown to diagnose bit-order issues.
3. Use **Frame** controls to step through animation frames or press **▶ Play**.
4. Adjust **Zoom** (1×–16×) to inspect individual pixels.
5. Toggle **Show pixel grid** to see pixel boundaries at high zoom.
6. The **Asset info** panel shows declared vs. expected byte counts and any warnings.

---

## Using the Panel Preview

Place a selected asset at arbitrary x/y coordinates within a 128×64 OLED canvas:

1. Select an asset.
2. Set **X** and **Y** (top-left of asset by default).
3. Change **Anchor** to align the asset differently.
4. Enable **Clipping** to cut the asset at screen boundaries.
5. Enable **Clear screen before frame** to see each frame in isolation.

---

## Using the Keyboard State panel

All controls immediately `PATCH /api/state` on the server. The state JSON is shown live at the bottom of the panel.

**Layer** buttons switch between the 7 real keymap layers.
**Keyboard capture** forwards physical key events from your PC to the simulator.

---

## Diagnostic guide

### Image appears shifted or offset
- Check `offset` in assets.json (default 0)
- Try adjusting X/Y in the Panel Preview tab
- For LVGL format: verify the 8-byte palette prefix is being stripped (check `format: "lvgl-indexed-1bit"`)

### Image appears squashed or stretched
- `width` and `height` in assets.json are incorrect
- Try incrementing/decrementing by 1 pixel to find the correct stride

### Image appears cut off
- `width` is too large for the actual pixel data
- Check the `rawByteCount` vs `pixelByteCount` in the info panel
- Verify `frames` count is correct (more frames = fewer bytes per frame)

### Image appears inverted (white/black swapped)
- Toggle **Invert colors** in the Asset Viewer
- Or use `CONFIG_NICE_OLED_WIDGET_INVERTED=y` equivalent setting

### Frames appear concatenated or apelotonados
- Verify `frames` count in assets.json
- Check `frameStride`: if frames are not equal-sized, set `frameStride` explicitly

### Wrong bit order (mirrored/garbled pixels)
- Try switching between `mono-horizontal-msb` and `mono-horizontal-lsb`
- Try `mono-vertical-pages-lsb` if the image appears as diagonal stripes

### Stride incorrect
- Set `stride` in assets.json to override the `ceil(width/8)` default
- Some tools pad rows to 4-byte alignment; try `stride: ceil(width/8 * 4) / 4 * 4`

### frameStride incorrect
- Set `frameStride` in assets.json. Value = bytes per single frame including any padding

---

## API reference

```
GET  /api/assets              List assets with metadata and summary
GET  /api/assets/:name        Asset detail + bytesBase64 for client rendering
GET  /api/state               Current keyboard state
PATCH /api/state              Partial state update (deep merge)
POST /api/key-event           Key event: { type: "tap"|"key_down"|"key_up"|"clear", key?: string }
POST /api/reload-assets       Reload assets.json and all source files (no restart needed)
POST /api/reset               Reset state to DEFAULT_STATE
```

### Example: set layer to UTILS (6) and low battery

```bash
curl -X PATCH http://localhost:5173/api/state \
  -H "Content-Type: application/json" \
  -d '{"keyboard":{"activeLayer":6},"battery":{"left":15,"right":22}}'
```

### Example: simulate a key tap

```bash
curl -X POST http://localhost:5173/api/key-event \
  -H "Content-Type: application/json" \
  -d '{"type":"tap","key":"A"}'
```

---

## Architecture

```
server.mjs          HTTP server + router + static file server
src/
  asset-loader.mjs  Read assets.json, resolve paths, parse, validate
  asset-parser.mjs  Parse C source files → extract byte arrays
  bitmap-renderer.mjs  Byte array → flat pixel array (pure function, all formats)
  state.mjs         Keyboard state: defaults, getState, patchState, processKeyEvent
public/
  index.html        Single-page app shell
  styles.css        Dark OLED theme
  app.js            Client: API client, BitmapDecoder, OLEDCanvas, AssetViewer,
                    PanelPreview, StatePanel — no build step required
test/
  asset-parser.test.mjs
  bitmap-renderer.test.mjs
  state.test.mjs
  asset-loader.test.mjs
assets.json         Asset manifest (edit this to add real firmware assets)
```

The bitmap interpretation is intentionally **duplicated** in `bitmap-renderer.mjs` (server-side, used in tests and validation) and `app.js` (client-side, used for zero-latency format switching in the UI without a server round-trip).
