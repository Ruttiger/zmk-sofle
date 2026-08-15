#!/usr/bin/env bash
set -euo pipefail

repo=""
zmk_root="$HOME/zmk"

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

source "$zmk_root/.venv/bin/activate"
export ZEPHYR_SDK_INSTALL_DIR="${ZEPHYR_SDK_INSTALL_DIR:-$HOME/zephyr-sdk-0.16.8}"

zmk_app="$zmk_root/app"
latest_dir="$repo/firmware/latest"
mkdir -p "$latest_dir"

build_variant() {
  local variant="$1"
  local height_label="$2"
  local build_dir="$zmk_root/build/ruttiger_eyelash_sofle_oled_res_${height_label}_left"
  local artifact="ruttiger_eyelash_sofle_oled_res_${height_label}_left.uf2"

  cd "$zmk_app"
  west build --pristine=always \
    -d "$build_dir" \
    -b eyelash_sofle_left \
    -- \
    -DSHIELD="eyelash_sofle_central_left oled_resolution_diag oled_resolution_diag_${variant}" \
    -DDTC_OVERLAY_FILE="$repo/config/eyelash_sofle.keymap" \
    -DZMK_CONFIG="$repo/config/oled_resolution_diag" \
    -DZMK_EXTRA_MODULES="$repo"

  cp "$build_dir/zephyr/zmk.uf2" "$latest_dir/$artifact"
  echo "Copied $latest_dir/$artifact"
}

build_variant 128x32 128x32
build_variant 128x64 128x64
