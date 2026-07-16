# TensorRT Hook

Reference implementation of the ML encryption hook for TensorRT models.

## Overview

This directory contains the canonical implementation of the encryption hook mechanism. It serves as the reference for implementing hooks in other frameworks (ONNX, OpenVINO, TFLite).

## Structure

```
tensorrt/
├── README.md           # This file
├── src/                # Source code
│   ├── compile.cpp             # Builds a TensorRT engine from ONNX, then encrypts it (AES-256-ECB)
│   ├── infer.cpp                # Loads the encrypted engine, decrypts it in memory, runs inference
│   └── trt_deserialize_hook.c   # LD_PRELOAD hook that dumps the decrypted engine before TensorRT uses it
├── examples/            # Sample .onnx models for testing
├── runtime/             # Generated encrypted engines (not versioned)
├── attack/              # Dumped (stolen) engines produced by the hook (not versioned)
└── tests/               # Test cases

```
## Requirements

- Linux (tested on Jetson, aarch64)
- `g++` / `gcc` with C++17 support
- NVIDIA TensorRT (`libnvinfer`, `libnvonnxparser`) and headers
- CUDA toolkit (`cudart`)
- OpenSSL development headers (`libssl-dev`, provides `libcrypto`)

On Jetson, header/library paths may differ from a standard install. If `cuda_runtime_api.h` is not found under `/usr/local/cuda/include`, locate it with:
```bash
find /usr -iname "cuda_runtime_api.h"
```
and adjust the `-I`/`-L` flags below accordingly (e.g. `/usr/local/cuda-12.6/targets/aarch64-linux/include`).

## Build

**1. Compile `compile` (builds + encrypts a TensorRT engine from an ONNX model):**
```bash
g++ compile.cpp -o compile \
    -I/usr/include/aarch64-linux-gnu \
    -I/usr/local/cuda-12.6/targets/aarch64-linux/include \
    -L/usr/lib/aarch64-linux-gnu \
    -L/usr/local/cuda-12.6/targets/aarch64-linux/lib \
    -lnvinfer -lnvonnxparser -lcudart -lcrypto
```

**2. Compile `infer` (loads + decrypts + runs the engine):**
```bash
g++ -std=c++17 infer.cpp -o infer \
    -I/usr/include/aarch64-linux-gnu \
    -I/usr/local/cuda-12.6/targets/aarch64-linux/include \
    -L/usr/lib/aarch64-linux-gnu \
    -L/usr/local/cuda-12.6/targets/aarch64-linux/lib \
    -lnvinfer -lcudart -lcrypto
```

**3. Compile the hook (`trt_deserialize_hook.so`):**
```bash
gcc -shared -fPIC trt_deserialize_hook.c -o trt_deserialize_hook.so \
    -I/usr/include/aarch64-linux-gnu -ldl
```

## Run

**1. Encrypt a model:**
```bash
./compile ../examples/model.onnx model.engine.enc <password>
```
Output is written to `../runtime/model.engine.enc`.

**2. Run inference normally (baseline, no attack):**
```bash
./infer ../runtime/model.engine.enc <password>
```

**3. Run the attack:**
```bash
LD_PRELOAD=./trt_deserialize_hook.so ./infer ../runtime/model.engine.enc <password>
```
The decrypted engine is dumped to `../attack/dumped.engine`, byte-for-byte identical to the in-memory decrypted buffer — despite the on-disk file being encrypted.

## How it works

1. `infer` reads the encrypted `.engine` file and decrypts it in RAM (AES-256-ECB, key derived from the password via SHA256).
2. It calls `runtime->deserializeCudaEngine(blob, size)` to load the decrypted buffer into TensorRT.
3. `deserializeCudaEngine` is a C++ virtual method. The hook intercepts the exported `createInferRuntime_INTERNAL` symbol via `LD_PRELOAD`, locates the internal implementation object's vtable, and overwrites the function pointer for `deserializeCudaEngine` with its own function.
4. When `infer` later calls `deserializeCudaEngine`, the hook's function runs first: it dumps the decrypted buffer to disk, then forwards the call to the real function so the program behaves normally and the victim notices nothing.

## References

- [TensorRT Documentation](https://docs.nvidia.com/deeplearning/tensorrt/)
- Parent project: [ML Encryption Hook Toolkit](../)
