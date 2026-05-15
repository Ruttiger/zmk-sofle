#!/usr/bin/env bash
set -euo pipefail

repo=""
zmk_root="$HOME/zmk"
zmk_revision="v0.3.0"

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
    --zmk-revision)
      zmk_revision="$2"
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

echo "== WSL ZMK setup =="
echo "Repo config: $repo"
echo "ZMK root: $zmk_root"
echo "ZMK revision: $zmk_revision"
echo ""

missing_packages=()
command -v git >/dev/null 2>&1 || missing_packages+=(git)
command -v python3 >/dev/null 2>&1 || missing_packages+=(python3)
command -v pip3 >/dev/null 2>&1 || missing_packages+=(python3-pip)
command -v cmake >/dev/null 2>&1 || missing_packages+=(cmake)
command -v ninja >/dev/null 2>&1 || missing_packages+=(ninja-build)
command -v dtc >/dev/null 2>&1 || missing_packages+=(device-tree-compiler)
command -v gperf >/dev/null 2>&1 || missing_packages+=(gperf)
command -v ccache >/dev/null 2>&1 || missing_packages+=(ccache)
command -v wget >/dev/null 2>&1 || missing_packages+=(wget)
command -v xz >/dev/null 2>&1 || missing_packages+=(xz-utils)
command -v file >/dev/null 2>&1 || missing_packages+=(file)

if [[ "${#missing_packages[@]}" -gt 0 ]]; then
  echo "Missing required system tools for local ZMK builds."
  echo "On Ubuntu, install them with:"
  echo "  sudo apt update"
  echo "  sudo apt install -y git python3 python3-pip python3.10-venv cmake ninja-build gperf ccache dfu-util device-tree-compiler wget xz-utils file make gcc gcc-multilib g++-multilib libsdl2-dev"
  echo ""
  echo "Missing commands/packages detected: ${missing_packages[*]}"
  exit 1
fi

mkdir -p "$(dirname "$zmk_root")"

if [[ ! -d "$zmk_root/.git" ]]; then
  echo "Cloning ZMK into $zmk_root"
  git clone --branch "$zmk_revision" https://github.com/zmkfirmware/zmk.git "$zmk_root"
else
  echo "ZMK checkout already exists."
  git -C "$zmk_root" fetch --tags
  git -C "$zmk_root" checkout "$zmk_revision"
fi

cd "$zmk_root"

if [[ -d ".venv" && ! -f ".venv/bin/activate" ]]; then
  echo "Found an incomplete Python virtual environment. Recreating .venv."
  rm -rf .venv
fi

if [[ ! -d ".venv" ]]; then
  echo "Creating Python virtual environment."
  if ! python3 -m venv .venv; then
    echo ""
    echo "Python venv creation failed. On Ubuntu 22.04, install the venv package and rerun setup:"
    echo "  sudo apt install -y python3.10-venv"
    exit 1
  fi
fi

# shellcheck source=/dev/null
source .venv/bin/activate
python -m pip install --upgrade pip wheel west
python -m pip install --upgrade "setuptools<81"

if [[ ! -d ".west" ]]; then
  west init -l app/
fi

west update
west zephyr-export
if west packages --help >/dev/null 2>&1; then
  west packages pip --install
else
  python -m pip install -r zephyr/scripts/requirements.txt -r app/scripts/requirements.txt
fi
python -m pip install --upgrade pyelftools
python -c "import pkg_resources" >/dev/null

echo ""
echo "ZMK setup finished."
echo "If the Zephyr SDK is not installed yet, install it inside WSL before building:"
echo "  bash '$repo/scripts/wsl/install-zephyr-sdk-wsl.sh'"
echo "Then validate with:"
echo "  bash '$repo/scripts/wsl/validate-wsl-zmk.sh' --repo '$repo' --zmk-root '$zmk_root'"
