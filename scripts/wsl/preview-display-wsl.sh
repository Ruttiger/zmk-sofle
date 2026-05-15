#!/usr/bin/env bash
# Preview the ZMK status screen in an SDL window (via WSLg or X11).
# Builds the eyelash_sofle_display_preview shield for native_sim/native/64,
# then launches the resulting binary so the display renders on the desktop.
#
# Usage:
#   bash preview-display-wsl.sh --repo <path> [--zmk-root <path>] [--no-rebuild]
#   bash preview-display-wsl.sh --repo <path> [--zmk-root <path>] --build-only
#   bash preview-display-wsl.sh --repo <path> [--zmk-root <path>] --launch-only
set -euo pipefail

repo=""
zmk_root="$HOME/zmk"
rebuild=true
build_only=false
launch_only=false

while [[ $# -gt 0 ]]; do
  case "$1" in
    --repo)
      repo="$2"
      shift 2
      ;;
    --zmk-root)
      zmk_root="$2"
      shift 2
      ;;
    --no-rebuild)
      rebuild=false
      shift
      ;;
    --build-only)
      build_only=true
      shift
      ;;
    --launch-only)
      launch_only=true
      shift
      ;;
    *)
      echo "Unknown argument: $1" >&2
      exit 2
      ;;
  esac
done

if [[ -z "$repo" ]]; then
  echo "Missing --repo" >&2
  exit 2
fi

if [[ "$zmk_root" == "~"* ]]; then
  zmk_root="${HOME}${zmk_root:1}"
fi

zmk_app="$zmk_root/app"
config_dir="$repo/config"
build_dir="$zmk_root/build/eyelash_sofle_display_preview"
binary="$build_dir/zephyr/zmk.exe"

# ── Launch-only path ──────────────────────────────────────────────────────────
# Called from Invoke-WslInteractive (inherited stdio) after the build phase.

if [[ "$launch_only" == "true" ]]; then
  if [[ ! -f "$binary" ]]; then
    echo "ERROR: Preview binary not found: $binary" >&2
    echo "  Build it first (run without --launch-only)." >&2
    exit 1
  fi
  if [[ -z "${DISPLAY:-}" && -z "${WAYLAND_DISPLAY:-}" ]]; then
    echo "ERROR: No graphical display detected (DISPLAY and WAYLAND_DISPLAY are unset)." >&2
    echo "  On Windows 11 WSLg sets this automatically." >&2
    echo "  On Windows 10, start VcXsrv/X410 and set:" >&2
    echo "    export DISPLAY=\$(grep nameserver /etc/resolv.conf | awk '{print \$2}'):0" >&2
    exit 1
  fi
  echo "Launching ZMK display preview..."
  echo "  SDL window: 160x68 px  (nice!view dimensions)"
  echo "  Close the window or press Ctrl+C to exit."
  # XShmPutImage crashes in WSL/XWayland when called from a background thread.
  # We intercept it via LD_PRELOAD and redirect to XPutImage (safe, no SHM).
  _workaround_c="$repo/scripts/wsl/xshm_workaround.c"
  _workaround_so="/tmp/xshm_workaround_zmk.so"
  if [[ ! -f "$_workaround_so" ]] && [[ -f "$_workaround_c" ]]; then
    gcc -shared -fPIC -O2 -o "$_workaround_so" "$_workaround_c" -lX11 2>/dev/null || _workaround_so=""
  fi
  export SDL_VIDEODRIVER=x11
  export SDL_RENDER_DRIVER=software
  [[ -n "${_workaround_so:-}" ]] && export LD_PRELOAD="$_workaround_so"
  # Redirect both fd 1 (stdout) and fd 2 (stderr) through a pipe so the
  # binary sees non-TTY file descriptors.  When wsl.exe is called from a
  # Windows Console, Zephyr native_posix detects a TTY on stderr and sets
  # up signal handlers that crash SDL/X11 on WSL.
  exec 1> >(cat)
  exec 2>&1
  exec "$binary"
fi

# ── Dependency checks ─────────────────────────────────────────────────────────

if [[ ! -d "$zmk_app" || ! -f "$zmk_app/CMakeLists.txt" ]]; then
  echo "ZMK app not found at $zmk_app." >&2
  echo "Run WSL setup first: scripts/Build-FirmwareLocal.ps1 -Setup" >&2
  exit 1
fi

if [[ -f "$zmk_root/.venv/bin/activate" ]]; then
  # shellcheck source=/dev/null
  source "$zmk_root/.venv/bin/activate"
fi

if ! command -v west >/dev/null 2>&1; then
  echo "west not found. Run WSL setup first." >&2
  exit 1
fi

# ── SDL software-renderer patch ───────────────────────────────────────────────
# SDL_RENDERER_ACCELERATED crashes in WSL/XWayland (no GPU for X11 apps).
# Patch display_sdl_bottom.c to use SDL_RENDERER_SOFTWARE before building,
# then restore the original file automatically on exit.
sdl_bottom="$zmk_root/zephyr/drivers/display/display_sdl_bottom.c"
sdl_patched=false

restore_sdl_bottom() {
  if [[ "$sdl_patched" == "true" ]] && [[ -f "$sdl_bottom" ]]; then
    ( cd "$zmk_root" && git checkout -- zephyr/drivers/display/display_sdl_bottom.c 2>/dev/null ) || true
  fi
}
trap restore_sdl_bottom EXIT

# SDL2 dev package required for compilation (provides runtime too)
if ! dpkg -s libsdl2-dev >/dev/null 2>&1 && ! ldconfig -p 2>/dev/null | grep -q 'libSDL2'; then
  echo "SDL2 not found." >&2
  echo "Install with: sudo apt-get install -y libsdl2-dev" >&2
  exit 1
fi

# A graphical display is required (WSLg on Win11 sets WAYLAND_DISPLAY/DISPLAY
# automatically; on Win10 set DISPLAY=<host-ip>:0 with VcXsrv/X410).
if [[ -z "${DISPLAY:-}" && -z "${WAYLAND_DISPLAY:-}" ]]; then
  echo "No graphical display detected (DISPLAY and WAYLAND_DISPLAY are unset)." >&2
  echo "On Windows 11 this is set automatically by WSLg." >&2
  echo "On Windows 10, start an X server (VcXsrv / X410) and set:" >&2
  echo "  export DISPLAY=\$(grep nameserver /etc/resolv.conf | awk '{print \$2}'):0" >&2
  exit 1
fi

# ── Build ─────────────────────────────────────────────────────────────────────

if [[ "$rebuild" == "true" ]]; then
  echo ""
  echo "== Building display preview (native_posix_64) =="
  if [[ -f "$sdl_bottom" ]]; then
    python3 "$repo/scripts/wsl/patch-sdl-bottom.py" "$sdl_bottom"
    sdl_patched=true
  fi
  cd "$zmk_app"
  west build --pristine=always \
    -d "$build_dir" \
    -b native_posix_64 -- \
    -DSHIELD="eyelash_sofle_display_preview" \
    -DZMK_CONFIG="$config_dir" \
    -DZMK_EXTRA_MODULES="$repo"
else
  echo "Skipping rebuild (--no-rebuild). Using existing binary."
fi

if [[ ! -f "$binary" ]]; then
  echo "Expected binary not found: $binary" >&2
  exit 1
fi

if [[ "$build_only" == "true" ]]; then
  echo ""
  echo "Build complete. Run 'Preview-Display.ps1 -NoRebuild' to launch the window."
  exit 0
fi

# ── Launch ────────────────────────────────────────────────────────────────────

echo ""
echo "Launching ZMK display preview..."
echo "  SDL window: 160×68 px  (nice!view dimensions)"
echo "  Close the window or press Ctrl+C to exit."
echo ""
# XShmPutImage crashes in WSL/XWayland when called from a background thread.
# Intercept via LD_PRELOAD and redirect to XPutImage.
_workaround_c="$repo/scripts/wsl/xshm_workaround.c"
_workaround_so="/tmp/xshm_workaround_zmk.so"
if [[ ! -f "$_workaround_so" ]] && [[ -f "$_workaround_c" ]]; then
  gcc -shared -fPIC -O2 -o "$_workaround_so" "$_workaround_c" -lX11 2>/dev/null || _workaround_so=""
fi
export SDL_VIDEODRIVER=x11
export SDL_RENDER_DRIVER=software
[[ -n "${_workaround_so:-}" ]] && export LD_PRELOAD="$_workaround_so"
# Redirect stdout+stderr through a pipe (see note in --launch-only).
exec 1> >(cat)
exec 2>&1
exec "$binary"
