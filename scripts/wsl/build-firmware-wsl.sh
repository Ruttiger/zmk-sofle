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

zmk_app="$zmk_root/app"
config_dir="$repo/config"
latest_dir="$repo/firmware/latest"

if [[ ! -d "$zmk_app" || ! -f "$zmk_app/CMakeLists.txt" ]]; then
  echo "ZMK app directory not found at $zmk_app."
  echo "Run setup first: scripts/Build-FirmwareLocal.ps1 -Setup"
  exit 1
fi

if [[ ! -d "$config_dir" ]]; then
  echo "Config directory not found: $config_dir" >&2
  exit 1
fi

if [[ -d "$zmk_root/.venv" && ! -f "$zmk_root/.venv/bin/activate" ]]; then
  echo "The Python virtual environment at $zmk_root/.venv is incomplete."
  echo "Run setup again to recreate it:"
  echo "  bash $repo/scripts/wsl/setup-zmk-wsl.sh --repo $repo --zmk-root $zmk_root"
  exit 1
fi

if [[ -f "$zmk_root/.venv/bin/activate" ]]; then
  # shellcheck source=/dev/null
  source "$zmk_root/.venv/bin/activate"
fi

if ! command -v west >/dev/null 2>&1; then
  echo "west is missing. Run setup first: scripts/Build-FirmwareLocal.ps1 -Setup" >&2
  exit 1
fi

if ! python -c "import elftools" >/dev/null 2>&1; then
  echo "Python package elftools is missing from the ZMK venv." >&2
  echo "Repair the venv dependencies with:" >&2
  echo "  cd $zmk_root" >&2
  echo "  source .venv/bin/activate" >&2
  echo "  python -m pip install -r zephyr/scripts/requirements.txt -r app/scripts/requirements.txt" >&2
  echo "  python -m pip install --upgrade pyelftools" >&2
  echo "Or rerun:" >&2
  echo "  bash $repo/scripts/wsl/setup-zmk-wsl.sh --repo $repo --zmk-root $zmk_root" >&2
  exit 1
fi

if ! python -c "import pkg_resources" >/dev/null 2>&1; then
  echo "Python package pkg_resources is missing from the ZMK venv." >&2
  echo "This ZMK/nanopb version needs setuptools with pkg_resources support." >&2
  echo "Repair it with:" >&2
  echo "  cd $zmk_root" >&2
  echo "  source .venv/bin/activate" >&2
  echo "  python -m pip install --upgrade 'setuptools<81'" >&2
  echo "Or rerun:" >&2
  echo "  bash $repo/scripts/wsl/setup-zmk-wsl.sh --repo $repo --zmk-root $zmk_root" >&2
  exit 1
fi

if ! command -v cmake >/dev/null 2>&1; then
  echo "cmake is missing inside WSL. Install local build dependencies:" >&2
  echo "  sudo apt update" >&2
  echo "  sudo apt install -y cmake ninja-build gperf ccache dfu-util device-tree-compiler wget xz-utils file make gcc gcc-multilib g++-multilib libsdl2-dev" >&2
  exit 1
fi

if ! command -v ninja >/dev/null 2>&1; then
  echo "ninja is missing inside WSL. Install local build dependencies:" >&2
  echo "  sudo apt update" >&2
  echo "  sudo apt install -y ninja-build" >&2
  exit 1
fi

if [[ -z "${ZEPHYR_SDK_INSTALL_DIR:-}" ]] && ! compgen -G "$HOME/zephyr-sdk-*" >/dev/null && ! compgen -G "$HOME/.local/opt/zephyr-sdk-*" >/dev/null && ! compgen -G "/opt/zephyr-sdk-*" >/dev/null; then
  echo "Zephyr SDK was not detected. Install it inside WSL:" >&2
  echo "  bash $repo/scripts/wsl/install-zephyr-sdk-wsl.sh" >&2
  exit 1
fi

mkdir -p "$latest_dir"

copy_uf2() {
  local build_dir="$1"
  local output_name="$2"
  local source="$build_dir/zephyr/zmk.uf2"

  if [[ ! -f "$source" ]]; then
    echo "Expected UF2 was not produced: $source" >&2
    exit 1
  fi

  cp "$source" "$latest_dir/$output_name"
  echo "Copied $latest_dir/$output_name"
}

run_build() {
  local label="$1"
  local build_dir="$2"
  shift 2

  echo ""
  echo "== Building $label =="
  cd "$zmk_app"
  west build --pristine=always -d "$build_dir" "$@"
}

run_build \
  "ruttiger_eyelash_sofle_standalone_left" \
  "$zmk_root/build/ruttiger_eyelash_sofle_standalone_left" \
  -b nice_nano_v2 -- \
  -DSHIELD="eyelash_sofle_central_left nice_view" \
  -DSNIPPET=studio-rpc-usb-uart \
  -DCONFIG_ZMK_STUDIO=y \
  -DCONFIG_ZMK_STUDIO_LOCKING=n \
  -DCONFIG_ZMK_SPLIT_ROLE_CENTRAL=y \
  -DZMK_CONFIG="$config_dir" \
  -DZMK_EXTRA_MODULES="$repo"
copy_uf2 "$zmk_root/build/ruttiger_eyelash_sofle_standalone_left" "ruttiger_eyelash_sofle_standalone_left.uf2"

run_build \
  "ruttiger_eyelash_sofle_standalone_right" \
  "$zmk_root/build/ruttiger_eyelash_sofle_standalone_right" \
  -b nice_nano_v2 -- \
  -DSHIELD="eyelash_sofle_peripheral_right nice_view" \
  -DZMK_CONFIG="$config_dir" \
  -DZMK_EXTRA_MODULES="$repo"
copy_uf2 "$zmk_root/build/ruttiger_eyelash_sofle_standalone_right" "ruttiger_eyelash_sofle_standalone_right.uf2"

run_build \
  "ruttiger_eyelash_sofle_settings_reset" \
  "$zmk_root/build/ruttiger_eyelash_sofle_settings_reset" \
  -b nice_nano_v2 -- \
  -DSHIELD=settings_reset \
  -DZMK_CONFIG="$config_dir" \
  -DZMK_EXTRA_MODULES="$repo"
copy_uf2 "$zmk_root/build/ruttiger_eyelash_sofle_settings_reset" "ruttiger_eyelash_sofle_settings_reset.uf2"

echo ""
echo "Local WSL firmware build finished."
