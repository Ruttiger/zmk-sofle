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

failures=0

check_cmd() {
  local name="$1"
  if command -v "$name" >/dev/null 2>&1; then
    echo "[OK] $name: $(command -v "$name")"
  else
    echo "[FAIL] $name is missing"
    failures=$((failures + 1))
  fi
}

check_path() {
  local path="$1"
  local label="$2"
  if [[ -e "$path" ]]; then
    echo "[OK] $label: $path"
  else
    echo "[FAIL] Missing $label: $path"
    failures=$((failures + 1))
  fi
}

echo "== WSL ZMK validation =="
echo "Repo: $repo"
echo "ZMK root: $zmk_root"
echo "Kernel: $(uname -a)"
echo ""

check_cmd git
check_cmd python3
check_cmd pip3
check_cmd cmake
check_cmd ninja

check_path "$repo/build.yaml" "repo build.yaml"
check_path "$repo/config/west.yml" "repo config/west.yml"
check_path "$repo/config/eyelash_sofle.keymap" "repo config/eyelash_sofle.keymap"
check_path "$repo/.github/workflows/build.yml" "repo GitHub workflow"
check_path "$repo/boards/arm/eyelash_sofle" "repo eyelash_sofle board module"
check_path "$zmk_root/app" "ZMK app directory"

if [[ -f "$zmk_root/.venv/bin/activate" ]]; then
  # shellcheck source=/dev/null
  source "$zmk_root/.venv/bin/activate"
  echo "[OK] ZMK Python venv: $zmk_root/.venv"
else
  echo "[FAIL] Missing ZMK Python venv: $zmk_root/.venv"
  failures=$((failures + 1))
fi

check_cmd west

if python -c "import elftools" >/dev/null 2>&1; then
  echo "[OK] Python package elftools"
else
  echo "[FAIL] Missing Python package elftools in ZMK venv"
  failures=$((failures + 1))
fi

if python -c "import pkg_resources" >/dev/null 2>&1; then
  echo "[OK] Python package pkg_resources"
else
  echo "[FAIL] Missing Python package pkg_resources in ZMK venv"
  failures=$((failures + 1))
fi

if [[ -d "$zmk_root/app" ]]; then
  if [[ -f "$zmk_root/app/CMakeLists.txt" ]]; then
    echo "[OK] ZMK app CMakeLists.txt found"
  else
    echo "[FAIL] $zmk_root/app does not look like a ZMK app directory"
    failures=$((failures + 1))
  fi
fi

if [[ -n "${ZEPHYR_SDK_INSTALL_DIR:-}" ]]; then
  echo "[OK] ZEPHYR_SDK_INSTALL_DIR=$ZEPHYR_SDK_INSTALL_DIR"
elif compgen -G "$HOME/zephyr-sdk-*" >/dev/null || compgen -G "$HOME/.local/opt/zephyr-sdk-*" >/dev/null || compgen -G "/opt/zephyr-sdk-*" >/dev/null; then
  echo "[OK] Zephyr SDK-like directory detected"
else
  echo "[WARN] Zephyr SDK was not detected from common locations."
  echo "       Install it with: bash scripts/wsl/install-zephyr-sdk-wsl.sh"
fi

echo ""
if [[ "$failures" -gt 0 ]]; then
  echo "Validation failed with $failures required issue(s)."
  exit 1
fi

echo "WSL ZMK validation passed."
