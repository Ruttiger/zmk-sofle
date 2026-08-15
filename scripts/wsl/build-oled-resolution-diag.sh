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

  case "$build_dir" in
    "$zmk_root"/build/ruttiger_eyelash_sofle_oled_res_128x32_left|\
    "$zmk_root"/build/ruttiger_eyelash_sofle_oled_res_128x64_left)
      ;;
    *)
      echo "Refusing to remove unexpected build directory: $build_dir" >&2
      exit 1
      ;;
  esac

  rm -rf "$build_dir"

  cd "$zmk_app"
  west build --pristine=always \
    -d "$build_dir" \
    -b eyelash_sofle_left \
    -- \
    -DSHIELD="eyelash_sofle_central_left oled_resolution_diag oled_resolution_diag_${variant}" \
    -DSNIPPET=studio-rpc-usb-uart \
    -DCONFIG_ZMK_STUDIO=y \
    -DCONFIG_ZMK_STUDIO_LOCKING=n \
    -DZMK_CONFIG="$repo/config" \
    -DZMK_EXTRA_MODULES="$repo"

  source_hash="$(sha256sum "$build_dir/zephyr/zmk.uf2" | awk '{print $1}')"
  cp "$build_dir/zephyr/zmk.uf2" "$latest_dir/$artifact"
  copied_hash="$(sha256sum "$latest_dir/$artifact" | awk '{print $1}')"
  if [[ "$source_hash" != "$copied_hash" ]]; then
    echo "SHA256 mismatch for $artifact" >&2
    exit 1
  fi
  echo "SHA256 source: $source_hash"
  echo "SHA256 copied: $copied_hash"
  echo "Copied $latest_dir/$artifact"
}

build_variant 128x32 128x32
build_variant 128x64 128x64
