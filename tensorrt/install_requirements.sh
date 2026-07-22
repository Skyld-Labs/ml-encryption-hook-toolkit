#!/bin/bash
set -e

echo "=== Checking TensorRT requirements (Jetson) ==="

# TensorRT and CUDA are not pip/apt-installable standalone on Jetson -- they come
# pre-installed via the JetPack SDK. This script verifies they're present and
# installs the remaining lightweight dependencies (OpenSSL, build tools).

echo "[1/3] Checking for TensorRT headers..."
if [ -f /usr/include/aarch64-linux-gnu/NvInfer.h ]; then
    echo "      Found: /usr/include/aarch64-linux-gnu/NvInfer.h"
else
    echo "      NOT FOUND under /usr/include/aarch64-linux-gnu/"
    echo "      Searching system-wide..."
    FOUND=$(find /usr -iname "NvInfer.h" 2>/dev/null | head -1)
    if [ -n "$FOUND" ]; then
        echo "      Found at: $FOUND"
        echo "      Adjust TRT_INC in src/Makefile if this differs from the default path."
    else
        echo "      TensorRT headers not found anywhere on this system."
        echo "      Install the JetPack SDK components for TensorRT via NVIDIA SDK Manager,"
        echo "      then re-run this script."
        exit 1
    fi
fi

echo "[2/3] Checking for CUDA runtime headers..."
if [ -f /usr/local/cuda-12.6/targets/aarch64-linux/include/cuda_runtime_api.h ]; then
    echo "      Found: /usr/local/cuda-12.6/targets/aarch64-linux/include/cuda_runtime_api.h"
else
    echo "      NOT FOUND at the default path. Searching system-wide..."
    FOUND=$(find /usr -iname "cuda_runtime_api.h" 2>/dev/null | head -1)
    if [ -n "$FOUND" ]; then
        echo "      Found at: $FOUND"
        echo "      Adjust TRT_INC/TRT_LIB in src/Makefile to match this CUDA version/path."
    else
        echo "      CUDA headers not found anywhere on this system."
        echo "      Install the JetPack SDK components for CUDA via NVIDIA SDK Manager,"
        echo "      then re-run this script."
        exit 1
    fi
fi

echo "[3/3] Installing OpenSSL dev headers and build tools..."
sudo apt update
sudo apt install -y libssl-dev build-essential

echo "=== TensorRT requirements OK ==="
echo "If any path above differs from the Makefile defaults, update TRT_INC/TRT_LIB"
echo "at the top of src/Makefile before building."