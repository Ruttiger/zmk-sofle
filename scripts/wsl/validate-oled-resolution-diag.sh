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

for variant in 128x32 128x64; do
  build_dir="$zmk_root/build/ruttiger_eyelash_sofle_oled_res_${variant}_left"
  echo "--- ${variant} CONFIG_NICE_OLED_ON ---"
  if grep -nE '^# CONFIG_NICE_OLED_ON is not set$|^CONFIG_NICE_OLED_ON=n$' \
      "$build_dir/zephyr/.config"; then
    :
  else
    echo "UNSET (no CONFIG_NICE_OLED_ON entry)"
  fi

  echo "--- ${variant} zephyr.dts SSD1306 ---"
  awk '/oled: ssd1306@3c {/,/inversion-on;/' "$build_dir/zephyr/zephyr.dts" |
    sed -e 's/< 0x80 >/<128>/g' \
        -e 's/< 0x20 >/<32>/g' \
        -e 's/< 0x40 >/<64>/g' \
        -e 's/< 0x1f >/<31>/g' \
        -e 's/< 0x3f >/<63>/g'

  echo "--- ${variant} forbidden implementation references ---"
  if grep -RniE --exclude='*.bin' --exclude='*.elf' \
      'zmk-nice-oled-128x64\.c|zmk-nice-oled|screen_peripheral\.c|(^|[/\\])screen\.c|rotate_canvas' \
      "$build_dir"; then
    echo "FORBIDDEN_REFERENCE_FOUND"
    exit 1
  else
    echo "NONE"
  fi

  echo "--- ${variant} final link flags ---"
  if grep -Rni --include='*.ninja' --include='*.map' --include='*.cmd' \
      'rotate_canvas|--wrap' "$build_dir"; then
    echo "FORBIDDEN_LINK_REFERENCE_FOUND"
    exit 1
  else
    echo "NONE"
  fi

  echo "--- ${variant} diagnostic source reference ---"
  grep -RniE --include='*.ninja' --include='*.cmake' --include='*.dts' --include='*.config' \
    'oled_resolution_diag/custom_status_screen\.c' "$build_dir" || true
done

echo "--- SHA256 ---"
sha256sum \
  "$repo/firmware/latest/ruttiger_eyelash_sofle_oled_res_128x32_left.uf2" \
  "$repo/firmware/latest/ruttiger_eyelash_sofle_oled_res_128x64_left.uf2"
