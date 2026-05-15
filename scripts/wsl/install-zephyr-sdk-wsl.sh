#!/usr/bin/env bash
set -euo pipefail

sdk_version="0.16.8"
install_dir="$HOME"
toolchain="arm-zephyr-eabi"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --sdk-version)
      sdk_version="$2"
      shift 2
      ;;
    --install-dir)
      install_dir="$2"
      shift 2
      ;;
    --toolchain)
      toolchain="$2"
      shift 2
      ;;
    *)
      echo "Unknown argument: $1" >&2
      exit 2
      ;;
  esac
done

sdk_name="zephyr-sdk-${sdk_version}"
archive="${sdk_name}_linux-x86_64.tar.xz"
base_url="https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v${sdk_version}"
install_path="${install_dir%/}/${sdk_name}"

echo "== Install Zephyr SDK =="
echo "SDK version: $sdk_version"
echo "Install path: $install_path"
echo "Toolchain: $toolchain"
echo ""

if [[ -d "$install_path" && -f "$install_path/cmake/Zephyr-sdkConfig.cmake" ]]; then
  echo "Zephyr SDK already appears to be installed at $install_path"
  exit 0
fi

mkdir -p "$install_dir"
cd "$install_dir"

if [[ ! -f "$archive" ]]; then
  echo "Downloading $archive"
  wget "${base_url}/${archive}"
fi

echo "Verifying checksum"
wget -O - "${base_url}/sha256.sum" | shasum --check --ignore-missing

if [[ ! -d "$install_path" ]]; then
  echo "Extracting $archive"
  tar xvf "$archive"
fi

cd "$install_path"
echo "Running Zephyr SDK setup"
./setup.sh -t "$toolchain" -h -c

echo ""
echo "Zephyr SDK installed."
echo "If CMake still cannot find it, export:"
echo "  export ZEPHYR_SDK_INSTALL_DIR='$install_path'"

