#!/usr/bin/env bash
# install_requirements.sh - Sets up the QNN SDK environment and Python
# dependencies needed to convert, compile, and run QNN models.
#
# This framework has extra requirements compared to onnx/openvino/tflite:
#   - QNN SDK (QAIRT) must already be downloaded and extracted
#   - The converter toolchain requires Python 3.10 specifically (ABI-locked
#     native extension: libPyIrGraph)
#   - qnn-model-lib-generator requires a `clang++` binary on PATH (looks for
#     clang++-9 first, falls back to plain clang++)
#
# Usage: source ./install_requirements.sh
# (must be sourced, not executed, so the exported variables and activated


# --- Adjust this if your QNN SDK lives elsewhere ---
QNN_SDK_ROOT_DEFAULT="$HOME/Documents/Skyld/qnn-sdk/qairt/2.29.0.241129"
export QNN_SDK_ROOT="${QNN_SDK_ROOT:-$QNN_SDK_ROOT_DEFAULT}"

if [ ! -d "$QNN_SDK_ROOT" ]; then
    echo "ERROR: QNN_SDK_ROOT not found at $QNN_SDK_ROOT"
    echo "Download and extract the QAIRT SDK first, then set QNN_SDK_ROOT accordingly."
    # shellcheck disable=SC2317
    return 1 2>/dev/null || exit 1
fi

echo "Using QNN_SDK_ROOT=$QNN_SDK_ROOT"

# --- QNN SDK environment variables ---
export PATH="$QNN_SDK_ROOT/bin/x86_64-linux-clang:$PATH"
export LD_LIBRARY_PATH="$QNN_SDK_ROOT/lib/x86_64-linux-clang:$LD_LIBRARY_PATH"
export PYTHONPATH="$QNN_SDK_ROOT/lib/python:$PYTHONPATH"

# --- Python 3.10 venv (required: the converter's native extension is
#     ABI-locked to Python 3.10) ---
VENV_DIR="$HOME/Documents/Skyld/qnn-sdk/venv310"

if [ ! -d "$VENV_DIR" ]; then
    echo "Creating Python 3.10 venv at $VENV_DIR..."
    if ! command -v python3.10 &> /dev/null; then
        echo "ERROR: python3.10 not found. Install it first, e.g.:"
        echo "  sudo add-apt-repository ppa:deadsnakes/ppa"
        echo "  sudo apt update && sudo apt install python3.10 python3.10-dev python3.10-venv"
        # shellcheck disable=SC2317
        return 1 2>/dev/null || exit 1
    fi
    python3.10 -m venv "$VENV_DIR"
fi

echo "Activating venv: $VENV_DIR"
# shellcheck source=/dev/null
source "$VENV_DIR/bin/activate"

# --- libpython3.10.so and libc++.so.1 must be resolvable at runtime ---
export LD_LIBRARY_PATH="/usr/lib/x86_64-linux-gnu:$LD_LIBRARY_PATH"

# --- clang++ shim: qnn-model-lib-generator looks for a `clang++` binary,
#     but Ubuntu only ships versioned binaries (clang++-14, etc.) ---
CLANG_SHIM_DIR="$HOME/Documents/Skyld/qnn-sdk/clang-shim"
mkdir -p "$CLANG_SHIM_DIR"

if [ ! -e "$CLANG_SHIM_DIR/clang++" ]; then
    CLANGPP_BIN=$(command -v clang++-14 || command -v clang++-16 || true)
    if [ -z "$CLANGPP_BIN" ]; then
        echo "ERROR: no clang++-14 or clang++-16 found. Install with:"
        echo "  sudo apt install clang-14"
        # shellcheck disable=SC2317
        return 1 2>/dev/null || exit 1
    fi
    ln -sf "$CLANGPP_BIN" "$CLANG_SHIM_DIR/clang++"
    echo "Created clang++ shim -> $CLANGPP_BIN"
fi

export PATH="$CLANG_SHIM_DIR:$PATH"

# --- Check/install QNN converter's Python dependencies ---
echo "Checking QNN Python dependencies..."
python3 "$QNN_SDK_ROOT/bin/check-python-dependency"

# --- ONNX package (not covered by check-python-dependency, needed
#     specifically for the ONNX frontend of the converter) ---
if ! python3 -c "import onnx" &> /dev/null; then
    echo "Installing onnx package..."
    pip install "onnx>=1.14,<1.17"
fi

echo ""
echo "QNN environment ready."
echo "  QNN_SDK_ROOT   = $QNN_SDK_ROOT"
echo "  Python         = $(python3 --version)"
echo "  clang++        = $(clang++ --version | head -1)"
echo ""
echo "You can now run: make -C src all"

set +e   
echo ""
echo "QNN environment ready."