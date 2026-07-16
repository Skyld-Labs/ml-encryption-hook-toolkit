# OpenVINO Hook

Implementation of the ML encryption hook for OpenVINO models.

## Overview

This directory contains the OpenVINO-specific implementation of the encryption hook mechanism, based on the TensorRT reference implementation.

## Structure

```
openvino/
├── README.md                     # This file
├── src/
│   ├── test_load.cpp             # Sanity check: loads an ONNX model with OpenVINO, prints IO info
│   ├── compile_ov.cpp            # Converts ONNX to OpenVINO IR (XML+weights), then encrypts it (AES-256-ECB)
│   ├── infer_ov.cpp              # Loads the encrypted IR, decrypts it in memory, runs inference
│   └── openvino_read_model_hook.c # LD_PRELOAD hook that dumps the decrypted IR before OpenVINO uses it
├── examples/                     # Sample .onnx models for testing
├── runtime/                      # Generated encrypted IR files (not versioned)
├── attack/                       # Dumped (stolen) IR files produced by the hook (not versioned)
└── tests/                        # Test cases
```
## Requirements

- Linux (tested on x86_64)
- `g++` with C++17 support
- OpenVINO Runtime 2024.6.0 (`libopenvino`) and headers, installed under `/opt/intel/openvino_2024.6.0`
- OpenSSL development headers (`libssl-dev`, provides `libcrypto`)

If OpenVINO is installed elsewhere, adjust the `-I`/`-L` flags below and the `LD_LIBRARY_PATH` export accordingly. You can locate the install with:
```bash
find /opt/intel -maxdepth 1 -iname "openvino*"
```

## Build

All commands below are run from `openvino/src`.

**0. Set up the library path (needed for every build/run step in this session):**
```bash
export LD_LIBRARY_PATH=/opt/intel/openvino_2024.6.0/runtime/lib/intel64:/opt/intel/openvino_2024.6.0/runtime/3rdparty/tbb/lib:$LD_LIBRARY_PATH
```
This tells the dynamic linker where to find OpenVINO's `.so` files, including dependencies such as TBB. It only affects the current terminal — add the same `export` line to `~/.bashrc` to persist it across sessions.

**1. Compile `test_load` (sanity check, loads an ONNX model with OpenVINO):**
```bash
g++ -std=c++17 test_load.cpp -o test_load \
    -I/opt/intel/openvino_2024.6.0/runtime/include \
    -L/opt/intel/openvino_2024.6.0/runtime/lib/intel64 \
    -lopenvino
```

**2. Compile `compile_ov` (converts ONNX to IR and encrypts it):**
```bash
g++ -std=c++17 compile_ov.cpp -o compile_ov \
    -I/opt/intel/openvino_2024.6.0/runtime/include \
    -L/opt/intel/openvino_2024.6.0/runtime/lib/intel64 \
    -lopenvino -lcrypto
```

**3. Compile `infer_ov` (loads + decrypts + runs the IR):**
```bash
g++ -std=c++17 infer_ov.cpp -o infer_ov \
    -I/opt/intel/openvino_2024.6.0/runtime/include \
    -L/opt/intel/openvino_2024.6.0/runtime/lib/intel64 \
    -lopenvino -lcrypto
```

**4. Compile the hook (`openvino_read_model_hook.so`):**

Note: despite its `.c` extension, this file is C++ (it uses `ov::`, `std::shared_ptr`, etc.), so it must be built with `g++`, not `gcc`.
```bash
g++ -std=c++17 -shared -fPIC openvino_read_model_hook.c -o openvino_read_model_hook.so \
    -I/opt/intel/openvino_2024.6.0/runtime/include \
    -L/opt/intel/openvino_2024.6.0/runtime/lib/intel64 \
    -lopenvino -ldl
```

## Run

**1. Encrypt a model:**
```bash
./compile_ov ../examples/model.onnx model.ov.enc <password>
```
Output is written to `../runtime/model.ov.enc`. This converts the ONNX model to OpenVINO IR (XML graph + binary weights), packs both into a single buffer (`[4-byte XML length][XML][weights]`), and encrypts it.

**2. Run inference normally (baseline, no attack):**
```bash
./infer_ov ../runtime/model.ov.enc <password>
```

**3. Run the attack:**
```bash
LD_PRELOAD=./openvino_read_model_hook.so ./infer_ov ../runtime/model.ov.enc <password>
```
The decrypted IR is dumped to `../attack/dumped_model.xml` and `../attack/dumped_weights.bin`, identical to the in-memory decrypted buffer — despite the on-disk file being encrypted.

## How it works

1. `infer_ov` reads the encrypted `.ov.enc` file and decrypts it in RAM (AES-256-ECB, key derived from the password via SHA-256), then splits it back into the IR XML string and the weights buffer.
2. It calls `core.read_model(xmlStr, weightsTensor)` to load the decrypted IR into OpenVINO.
3. `ov::Core::read_model(const std::string&, const ov::Tensor&) const` is the hook target. Unlike the TensorRT hook (which patches a C++ vtable pointer at runtime), this hook intercepts the exported symbol directly: it resolves the exact mangled symbol name via `__asm__("_Z...")` and `dlsym(RTLD_NEXT, ...)`, since the OpenVINO ABI is stable enough to link against directly through `LD_PRELOAD`.
4. When `infer_ov` calls `read_model`, the hook's function runs first: it dumps the XML and weights buffers to disk, then forwards the call to the real `read_model` so the program behaves normally and the victim notices nothing.

The exact mangled symbol name is version-specific. If OpenVINO is updated, re-verify it with:
```bash
nm -D /opt/intel/openvino_2024.6.0/runtime/lib/intel64/libopenvino.so | grep read_model
```

## References

- [OpenVINO Documentation](https://docs.openvino.ai/)
- [Reference Implementation](../tensorrt/)
- Parent project: [ML Encryption Hook Toolkit](../)