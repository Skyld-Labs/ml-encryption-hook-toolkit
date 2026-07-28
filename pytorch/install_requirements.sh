#!/usr/bin/env bash
# install_requirements.sh - Checks/prepares the LibTorch environment.
#
# LibTorch itself lives OUTSIDE this repo (like qnn-sdk/), since it's a
# multi-hundred-MB SDK with thousands of headers. Default expected location:
#   ~/Documents/Skyld/libtorch
#
# Usage: ./install_requirements.sh   (can be run directly, doesn't need sourcing
#         - unlike QNN's script, nothing here needs to persist in the shell)

set -e

LIBTORCH_ROOT_DEFAULT="$HOME/Documents/Skyld/libtorch"
LIBTORCH_ROOT="${LIBTORCH_ROOT:-$LIBTORCH_ROOT_DEFAULT}"

echo "Checking for LibTorch at: $LIBTORCH_ROOT"

if [ ! -d "$LIBTORCH_ROOT" ]; then
    echo "LibTorch not found. Downloading CPU-only build..."
    mkdir -p "$(dirname "$LIBTORCH_ROOT")"
    cd "$(dirname "$LIBTORCH_ROOT")"

    LIBTORCH_URL="https://download.pytorch.org/libtorch/cpu/libtorch-cxx11-abi-shared-with-deps-2.5.1%2Bcpu.zip"
    wget "$LIBTORCH_URL" -O libtorch.zip
    unzip -q libtorch.zip
    rm libtorch.zip
    echo "LibTorch extracted to $LIBTORCH_ROOT"
else
    echo "Found existing LibTorch installation."
fi

if [ ! -f "$LIBTORCH_ROOT/lib/libtorch_cpu.so" ]; then
    echo "ERROR: $LIBTORCH_ROOT does not look like a valid LibTorch install"
    echo "(libtorch_cpu.so not found in lib/)"
    exit 1
fi

# --- OpenSSL dev headers for the encrypt/decrypt/hook C++ code ---
if ! dpkg -s libssl-dev &> /dev/null; then
    echo "Installing libssl-dev..."
    sudo apt update && sudo apt install -y libssl-dev
fi

# --- g++ with C++17 support ---
if ! g++ -std=c++17 -x c++ -c /dev/null -o /dev/null &> /dev/null; then
    echo "ERROR: g++ does not support C++17. Install a recent g++ (build-essential)."
    exit 1
fi

echo ""
echo "PyTorch/LibTorch environment ready."
echo "  LIBTORCH_ROOT = $LIBTORCH_ROOT"
echo "  g++           = $(g++ --version | head -1)"
echo ""
echo "If you didn't use the default path, export LIBTORCH_ROOT before running make:"
echo "  export LIBTORCH_ROOT=/your/path/to/libtorch"
echo ""
echo "You can now run: make -C src all"

