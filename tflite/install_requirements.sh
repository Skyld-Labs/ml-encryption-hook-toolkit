#!/bin/bash
set -e

echo "=== Installing TensorFlow Lite C API and dependencies ==="

# Install build tools and OpenSSL for AES encryption
sudo apt update
sudo apt install -y libssl-dev build-essential cmake git

TF_VERSION="v2.18.0"
TF_SRC_DIR="$HOME/Downloads/tensorflow"
TF_BUILD_DIR="$HOME/Downloads/tflite_build"

# Clone TensorFlow sources (shallow clone: only the target tag, no full history)
if [ ! -d "$TF_SRC_DIR" ]; then
    echo "Cloning TensorFlow ${TF_VERSION}..."
    git clone https://github.com/tensorflow/tensorflow.git --branch "$TF_VERSION" --depth 1 "$TF_SRC_DIR"
else
    echo "TensorFlow sources already present at $TF_SRC_DIR."
fi

# Build only the TFLite subset (not all of TensorFlow) via its standalone CMake project.
# -DBUILD_SHARED_LIBS=ON is required: without it, CMake only produces static .a libraries,
# which cannot be targeted with LD_PRELOAD.
mkdir -p "$TF_BUILD_DIR"
cd "$TF_BUILD_DIR"
cmake "$TF_SRC_DIR/tensorflow/lite" -DTFLITE_ENABLE_XNNPACK=ON -DBUILD_SHARED_LIBS=ON
cmake --build . -j"$(nproc)"

echo "=== TensorFlow Lite built successfully ==="
echo "Sources: $TF_SRC_DIR"
echo "Build output (headers are read directly from sources, library is here): $TF_BUILD_DIR"
echo ""
echo "To compile, use:"
echo "  g++ -std=c++17 infer_tflite.cpp -o infer_tflite \\"
echo "      -I$TF_SRC_DIR \\"
echo "      -L$TF_BUILD_DIR \\"
echo "      -Wl,-rpath,$TF_BUILD_DIR \\"
echo "      -ltensorflow-lite -lcrypto"