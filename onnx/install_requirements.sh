#!/bin/bash
set -e

echo "=== Installing ONNX Runtime and dependencies ==="

# Install OpenSSL for AES encryption
sudo apt update
sudo apt install -y libssl-dev build-essential

# Download and install ONNX Runtime
cd ~/Downloads

# Check if ONNX Runtime is already downloaded
ONNX_VERSION="1.20.0"
ONNX_FILE="onnxruntime-linux-x64-${ONNX_VERSION}.tgz"

if [ ! -f "$ONNX_FILE" ]; then
    echo "Downloading ONNX Runtime ${ONNX_VERSION}..."
    curl -L "https://github.com/microsoft/onnxruntime/releases/download/v${ONNX_VERSION}/onnxruntime-linux-x64-${ONNX_VERSION}.tgz" -o "$ONNX_FILE"
else
    echo "ONNX Runtime ${ONNX_VERSION} already downloaded."
fi

# Extract and install
echo "Extracting ONNX Runtime..."
tar -xf "$ONNX_FILE"

# Create installation directory
sudo mkdir -p /opt/onnxruntime
sudo cp -r onnxruntime-linux-x64-${ONNX_VERSION}/* /opt/onnxruntime/

# Set up library path
echo "export LD_LIBRARY_PATH=/opt/onnxruntime/lib:\$LD_LIBRARY_PATH" >> ~/.bashrc
export LD_LIBRARY_PATH=/opt/onnxruntime/lib:$LD_LIBRARY_PATH

echo "=== ONNX Runtime installed successfully ==="
echo "Location: /opt/onnxruntime"
echo "Headers: /opt/onnxruntime/include"
echo "Libraries: /opt/onnxruntime/lib"
echo ""
echo "To compile, use:"
echo "  g++ -std=c++17 compile_onnx.cpp -o compile_onnx \\"
echo "      -I/opt/onnxruntime/include \\"
echo "      -L/opt/onnxruntime/lib \\"
echo "      -Wl,-rpath,/opt/onnxruntime/lib \\"
echo "      -lonnxruntime -lcrypto"