# ONNX Runtime Hook

Implementation of the ML encryption hook for ONNX Runtime models.

## Overview

This directory contains the ONNX Runtime-specific implementation of the encryption hook mechanism, based on the TensorRT reference implementation. Unlike TensorRT and OpenVINO, ONNX Runtime loads `.onnx` bytes directly — no intermediate conversion (engine build, IR serialization) is needed, which makes this the simplest of the three pipelines.

## Structure

```
onnx/
├── README.md                # This file
├── src/
│   ├── compile_onnx.cpp     # Encrypts a raw .onnx file (AES-256-ECB)
│   ├── infer_onnx.cpp       # Loads the encrypted model, decrypts it in memory, runs inference
│   └── onnx_session_hook.c  # LD_PRELOAD hook that dumps the decrypted model before ONNX Runtime uses it
├── examples/                # Sample .onnx models for testing
├── runtime/                 # Generated encrypted models (not versioned)
├── attack/                  # Dumped (stolen) models produced by the hook (not versioned)
└── tests/                   # Test cases
```

## Requirements

- Linux (tested on x86_64)
- `g++` / `gcc` with C++17 support
- ONNX Runtime (`libonnxruntime`) and headers, installed under `/opt/onnxruntime`
- OpenSSL development headers (`libssl-dev`, provides `libcrypto`)

If ONNX Runtime is installed elsewhere, adjust the `-I`/`-L` flags below accordingly.

## Build

All commands below are run from `onnx/src`.

**1. Compile `compile_onnx` (encrypts a raw `.onnx` file):**
```bash
g++ -std=c++17 compile_onnx.cpp -o compile_onnx \
    -I/opt/onnxruntime/include \
    -L/opt/onnxruntime/lib \
    -Wl,-rpath,/opt/onnxruntime/lib \
    -lonnxruntime -lcrypto
```

**2. Compile `infer_onnx` (loads + decrypts + runs the model):**
```bash
g++ -std=c++17 infer_onnx.cpp -o infer_onnx \
    -I/opt/onnxruntime/include \
    -L/opt/onnxruntime/lib \
    -Wl,-rpath,/opt/onnxruntime/lib \
    -lonnxruntime -lcrypto
```

**3. Compile the hook (`onnx_session_hook.so`):**
```bash
gcc -shared -fPIC onnx_session_hook.c -o onnx_session_hook.so \
    -I/opt/onnxruntime/include -ldl
```

## Run

**0. Make sure the output directories exist (first run only):**
```bash
mkdir -p ../runtime ../attack
```

**1. Encrypt a model:**
```bash
./compile_onnx ../examples/model.onnx model.onnx.enc <password>
```
Output is written to `../runtime/model.onnx.enc`.

**2. Run inference normally (baseline, no attack):**
```bash
./infer_onnx ../runtime/model.onnx.enc <password>
```

**3. Run the attack:**
```bash
LD_PRELOAD=./onnx_session_hook.so ./infer_onnx ../runtime/model.onnx.enc <password>
ls -la ../attack/
```
The decrypted model is dumped to `../attack/dumped_model.onnx`, byte-for-byte identical to the original `.onnx` file — despite the on-disk file being encrypted. You can verify this with:
```bash
diff ../attack/dumped_model.onnx ../examples/model.onnx
```

## How it works

1. `infer_onnx` reads the encrypted `.onnx.enc` file and decrypts it in RAM (AES-256-ECB, key derived from the password via SHA-256).
2. It constructs an `Ort::Session(env, model_data, model_data_length, options)`, which is the C++ wrapper's entry point for loading a model from an in-memory buffer. Internally, this calls the C API function `OrtApi::CreateSessionFromArray`.
3. ONNX Runtime exposes a stable, documented C API: `OrtGetApiBase()` is the single exported symbol, returning an `OrtApiBase*` whose `GetApi()` returns an `OrtApi*` — a plain struct of function pointers. This is a much more stable hook surface than a mangled C++ vtable (TensorRT) or a mangled symbol name (OpenVINO): the struct layout and field names are part of the public, versioned C API.
4. The hook re-exports `OrtGetApiBase()` itself (via `LD_PRELOAD`), resolves the real one with `dlsym(RTLD_NEXT, "OrtGetApiBase")`, then patches the `CreateSessionFromArray` field of the returned `OrtApi` struct to point to its own function.
5. When `infer_onnx` later triggers `CreateSessionFromArray`, the hook's function runs first: it dumps the decrypted model buffer to disk, then forwards the call to the real function so the program behaves normally and the victim notices nothing.

### Why `RTLD_NEXT` instead of `dlopen`

An earlier version of the hook used `dlopen("libonnxruntime.so", ...)` + `dlsym` to resolve the real `OrtGetApiBase`. This is fragile in an `LD_PRELOAD` context: if the dynamic linker's search path doesn't happen to resolve the library name, `dlopen` can fail silently, leading to a NULL function pointer call (segfault) when the real function is invoked. `dlsym(RTLD_NEXT, ...)` is the correct tool for this: it asks the dynamic linker to resolve the symbol from the libraries loaded *after* this one in the process, i.e. directly from the real `libonnxruntime.so` already mapped into the process — no extra `dlopen` needed, and no dependency on library search paths.

## References

- [ONNX Runtime C API Documentation](https://onnxruntime.ai/docs/api/c/)
- [Reference Implementation](../tensorrt/)
- Parent project: [ML Encryption Hook Toolkit](../)