# OLED Panel Emulator — Sofle ZMK

Local web tool for previewing OLED panel graphics without compiling or flashing firmware.
Inspect raw C bitmaps, simulate keyboard state, and test display layouts — all in a browser.

---

## Quick start

```bash
cd tools/panel-emulator
npm install
npm start
```

Open **http://localhost:5173** in any browser.

No build step, no configuration. Stop it with `Ctrl+C`.

---

## What you see on first load

The app has three tabs at the top:

```
[ Asset Viewer ]  [ Panel Preview ]  [ Keyboard State ]
```

**Asset Viewer** is where you spend most of your time. It loads immediately with the first asset in
`assets.json` selected. Two built-in test assets ship out of the box:

| Asset name | Description |
|---|---|
| `placeholder_black` | 160×68 solid black — canvas should look completely black |
| `placeholder_white` | 160×68 solid white — canvas should look completely white |

If the canvas looks black with `placeholder_black` selected, everything is working correctly.

---

## Asset Viewer — controls explained

```
Asset:  [ placeholder_black ▼ ]   Format: [ lvgl-indexed-1bit ▼ ]   [⟳ Reload]

        ┌─────────────────────────────────────────────────────────────┐
        │                                                             │
        │             (pixel canvas)                                  │
        │                                                             │
        └─────────────────────────────────────────────────────────────┘

Frame:  [◀]  1 / 1  [▶]   [▶ Play]   FPS: [12 ▼]

Zoom: 1× 2× 4× 8× 16×    □ Grid    □ Invert    □ Mirror H    □ Mirror V
Rotation: 0° 90° 180° 270°    □ Bounding box
Pixel color: [■ White ▼]    Background: [■ Black ▼]
```

### Asset selector
Dropdown lists every entry in `assets.json`. Changing the selection re-renders immediately.

### Format selector
The format declared in `assets.json` is pre-selected. Change it without editing any file — useful
when you are not sure which bit order a source file uses:

| Format | When to use |
|---|---|
| `lvgl-indexed-1bit` | 8-byte palette prefix + 1bpp horizontal MSB. **Used by all firmware assets here.** |
| `mono-horizontal-msb` | 1bpp rows, MSB leftmost. Same as LVGL without palette strip. |
| `mono-horizontal-lsb` | 1bpp rows, LSB leftmost. Try if the image looks horizontally mirrored. |
| `mono-vertical-pages-lsb` | OLED page layout (8px tall pages). Try if the image looks like diagonal stripes. |

### Frame navigation
Use `◀` / `▶` to step through frames, or click **▶ Play** for continuous playback.
The FPS dropdown sets speed (1–60 fps).

### Zoom + grid
Zoom from 1× (actual pixels) to 16× (big enough to see individual bits).
Enable **Grid** at 4× or higher to see pixel boundaries.

### Invert / Mirror
- **Invert** flips black↔white, matching `CONFIG_NICE_OLED_WIDGET_INVERTED=y` in firmware.
- **Mirror H / V** flips the canvas horizontally or vertically.

### Rotation
Rotates the displayed canvas 0°/90°/180°/270°. The firmware internally renders at 90° (left half)
or 270° (right half) before sending to the 128×64 hardware. If you are checking a pre-rotation
asset, set Rotation to 90° to see it upright.

### Asset info panel
Shows byte counts and any warnings. Green "OK" = parsed bytes match declared dimensions exactly.
Orange = mismatch. See [Understanding warnings](#understanding-warnings) below.

---

## Panel Preview tab

Place an asset at specific coordinates within a 128×64 canvas — matching the physical OLED:

1. Select an asset in the **Asset** dropdown.
2. Set **X** and **Y** to position the top-left corner.
3. Change **Anchor** to align by center, bottom-right, etc.
4. Enable **Clipping** to crop the asset at screen edges.
5. Enable **Clear screen** to see each animation frame without ghosting.

Use this tab to check how an icon or animation lines up with real OLED boundaries.

---

## Keyboard State tab

Shows the current simulated keyboard state. All controls update live:

- **Layer buttons** — WINDOWS, WINDOWS_FN, MAC, MAC_FN, GALLIUM, GALLIUM_FN, UTILS.
- **Battery sliders** — simulate battery level and charging state.
- **BLE toggles** — simulate connected/disconnected profiles.
- **Keyboard capture** — forwards real keystrokes from your PC to the simulator.
- **⟳ Reset** — restores all defaults.

The raw JSON state is shown at the bottom of the panel.

---

## Reloading assets without restarting

When you add or edit an entry in `assets.json`, click **⟳ Reload assets** in the Asset Viewer, or:

```bash
curl -X POST http://localhost:5173/api/reload-assets
```

No server restart required.

---

## Loading your first real asset

### Step 1 — Clone the external module

The cat, dog, crystal, spaceman, and pokemon assets are not in this repo. They live in the
external `zmk-nice-oled` module. Clone it next to this workspace:

```bash
# Run from the workspace root (zmk-sofle/)
git clone -b panel_editions_ssd1306_fix \
  https://github.com/Ruttiger/zmk-nice-oled \
  ../zmk-nice-oled
```

### Step 2 — Inspect the source file

Open one of the C files to see the symbol name and dimensions:

```bash
# Example: right-half cat animation
cat ../zmk-nice-oled/boards/shields/nice_oled/widgets/images/cat_0.c
```

Look for:
- The array name: `static const uint8_t cat_0_map[] = {`
- The `lv_img_dsc_t` struct that declares `.header.w` and `.header.h`

### Step 3 — Add an entry to `assets.json`

Open `tools/panel-emulator/assets.json` and add your entry inside the `"assets"` array:

```json
{
  "name":   "cat_frame_0",
  "source": "../../../zmk-nice-oled/boards/shields/nice_oled/widgets/images/cat_0.c",
  "symbol": "cat_0_map",
  "width":  32,
  "height": 32,
  "frames": 1,
  "format": "lvgl-indexed-1bit",
  "notes":  "Right-half cat animation, frame 0 of 8"
}
```

`source` is always **relative to `tools/panel-emulator/`**.

If a single C file contains all frames concatenated into ONE array, set `"frames": 8`.
If each frame is a separate C file (like the cat animation), add one entry per frame — or
one entry per file with `"frames": 1`.

### Step 4 — Reload

Click **⟳ Reload assets** in the browser. The new asset appears in the dropdown immediately.

---

## All animation assets at a glance

| Asset set | Location in `zmk-nice-oled` | Frames | Used on |
|---|---|---|---|
| Cat | `widgets/images/cat_0.c` … `cat_7.c` | 8 (one per file) | Right half, active |
| Luna (dog) | `widgets/images/dog_sit1_90.c` … | 8 (one per file) | Left half, WPM-reactive |
| Crystal | `widgets/images/crystal_01.c` … `crystal_16.c` | 16 | Fallback |
| Head | `widgets/images/head_00.c` … `head_15.c` | 16 | — |
| Spaceman | `widgets/images/spaceman_00.c` … | 20 | — |
| Pokemon | `widgets/images/pokemon_00.c` … | 48 | — |
| Static | `widgets/images/vim.c`, `vip_marcos.c` | 1 | — |

> The Luna assets are pre-rotated 90°. Use **Rotation: 90°** in the Asset Viewer to see them upright.

---

## Understanding warnings

Warnings appear in orange in the **Asset info** panel. The image still renders — warnings are not errors.

### "Expected N pixel bytes, got M"

The parsed byte count does not match what `width × height × frames` predicts.

- **M < N (got fewer bytes)**: `frames` is set too high, or the file has slightly fewer bytes than
  expected. The image renders but may be cut off at the bottom.  
  Fix: lower `frames`, or check the actual byte count in the C file.

- **M > N (got more bytes)**: the file has more data than one frame declares. `frames` may be
  set too low.  
  Fix: raise `frames`, or check if the file contains concatenated frames.

### "Symbol not found"

The `symbol` field does not match any array name in the source file. Check spelling — it is
case-sensitive. Run `listSymbols` in the browser console to see all symbols in the loaded file.

### "Format auto-detected as …"

`format` was omitted and the parser guessed from context. Usually correct. Set `format` explicitly
to suppress the warning.

---

## `assets.json` field reference

| Field | Required | Description |
|---|---|---|
| `name` | yes | Unique identifier shown in the UI dropdown |
| `source` | yes | Path to C source file, relative to `tools/panel-emulator/` |
| `symbol` | yes | Name of the C byte array (e.g. `cat_0_map`) |
| `width` | yes | Pixel width |
| `height` | yes | Pixel height |
| `frames` | no | Number of animation frames concatenated in one array (default: 1) |
| `format` | no | Bitmap format (default: `lvgl-indexed-1bit`) |
| `stride` | no | Bytes per row — overrides the `ceil(width/8)` default |
| `frameStride` | no | Bytes per frame — overrides automatic calculation |
| `offset` | no | Byte offset into the array before pixel data starts |
| `notes` | no | Free-text annotation shown in the UI |

---

## Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| Canvas is all black | `placeholder_black` selected — correct | Switch to `placeholder_white` to confirm rendering works |
| Canvas is all white | `placeholder_white` selected — correct | — |
| Image looks like random noise | Wrong format selected | Try switching between `lvgl-indexed-1bit` and `mono-horizontal-msb` |
| Image appears horizontally mirrored | Wrong bit order | Try `mono-horizontal-lsb` |
| Image looks like diagonal stripes | Wrong pixel layout | Try `mono-vertical-pages-lsb` |
| Image appears squashed or stretched | Wrong `width` or `height` | Check the `lv_img_dsc_t` struct in the C file |
| Image cut off at the bottom | Byte count short | Lower `frames` or verify actual data in C file |
| "Symbol not found" warning | Wrong `symbol` name | Copy the exact array name from the C declaration |
| Asset not in dropdown after editing | `assets.json` not reloaded | Click **⟳ Reload assets** |
| Server won't start — port in use | Port 5173 taken | `PORT=3000 npm start` |

---

## API quick reference

```
GET  /api/assets              List all declared assets
GET  /api/assets/:name        Asset bytes + metadata (bytesBase64 for rendering)
GET  /api/state               Current keyboard state JSON
PATCH /api/state              Partial state update
POST /api/reload-assets       Re-read assets.json and all C source files
POST /api/reset               Reset keyboard state to defaults
POST /api/key-event           Simulate key: { "type": "tap", "key": "A" }
```

Example — set layer 6 (UTILS) with low battery:

```bash
curl -X PATCH http://localhost:5173/api/state \
  -H "Content-Type: application/json" \
  -d '{"keyboard":{"activeLayer":6},"battery":{"left":10,"right":8}}'
```

---

## Run tests

```bash
npm test
```

Covers the asset parser, bitmap renderer, and state machine. No browser needed.

---

## Architecture

```
server.mjs               HTTP server (node:http, port 5173)
src/
  asset-loader.mjs       Read assets.json, resolve paths, parse, validate byte counts
  asset-parser.mjs       Extract uint8_t arrays from C source text
  bitmap-renderer.mjs    Bytes to pixel array (pure function, all formats)
  state.mjs              Keyboard state: defaults, getState, patchState
public/
  index.html             Single-page app (3 tabs)
  styles.css             Dark OLED theme
  app.js                 Client-side rendering: BitmapDecoder, OLEDCanvas,
                         AssetViewer, PanelPreview, StatePanel
test/
  *.test.mjs             Unit tests (node:test, no browser needed)
assets.json              Asset manifest — edit this to add firmware assets
```

```
[ Asset Viewer ]  [ Panel Preview ]  [ Keyboard State ]
```

**Asset Viewer** is where you spend most of your time. It loads immediately with the first asset in `assets.json` selected. Two built-in test assets ship out of the box:

| Asset name | Description |
|---|---|
| `placeholder_black` | 160x68 solid black — canvas should look completely black |
| `placeholder_white` | 160x68 solid white — canvas should look completely white |

If the canvas looks black with `placeholder_black` selected, everything is working correctly.
