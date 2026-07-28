# ml-encryption-hook-toolkit

A toolkit demonstrating runtime extraction of encrypted ML models across six inference frameworks.

This repository collects proof-of-concept attacks that capture plaintext model data from the runtime boundary of each framework, even when the on-disk model is stored encrypted.

## Frameworks covered

| Framework | Encrypted artifact | Hook surface | Special note |
|---|---|---|---|
| TensorRT | TensorRT engine binary | `deserializeCudaEngine` vtable slot | Reference implementation, Jetson/CUDA native | 
| OpenVINO | IR XML + weights buffer | `ov::Core::read_model(...)` mangled symbol | IR and weights must be captured together | 
| ONNX Runtime | `.onnx` bytes | `OrtGetApiBase()` / `OrtApi` patch | Same hook works for both C and C++ victims | 
| TensorFlow Lite | FlatBuffer model | `TfLiteModelCreate` C API | Simplest direct C function interception | 
| PyTorch (LibTorch) | TorchScript `.pt` bytes | `torch::jit::load(std::istream&)` C++ symbol | Symbol interposition on exported mangled symbol | 
| QNN | QNN context binary | `QnnContext_createFromBinary()` via `dlsym` / function table patch | Multi-stage conversion pipeline, Python 3.10 + GPU backend required |

## Repository structure

- `tensorrt/` — TensorRT proof-of-concept
- `openvino/` — OpenVINO proof-of-concept
- `onnx/` — ONNX Runtime proof-of-concept
- `tflite/` — TensorFlow Lite proof-of-concept
- `pytorch/` — PyTorch / LibTorch proof-of-concept
- `qnn/` — Qualcomm QNN proof-of-concept

Each framework directory contains:

- `README.md` — framework-specific documentation
- `install_requirements.sh` — framework dependency setup
- `src/` — source code and build logic
- `build/` — generated binaries (git-ignored)
- `models/` — model artifacts and outputs (mostly git-ignored except source models and `.gitkeep` placeholders)

## Getting started

For each framework, follow the instructions in its own `README.md`. Most frameworks expose the same workflow:

1. Install dependencies with `./install_requirements.sh`
2. Build the demo from `src/` with `make all`
3. Run `make run-baseline` to verify normal inference
4. Run `make run-attack` to exercise the hook and capture the stolen model

## Repository hygiene

The repo intentionally tracks only source files and structural placeholders. Generated build artifacts and encrypted/dumped model outputs are ignored using the root `.gitignore`:

- `**/build/*` except `**/build/.gitkeep`
- `**/models/*.enc`
- `**/models/dumped_*`
- `**/models/*.bin`, `**/models/*.xml`, `**/models/*.tflite`, `**/models/*.engine`, `**/models/*.onnx`
- framework-specific generated artifacts such as `qnn/models/model_patched.onnx`

This means the workspace can be cleaned and rebuilt safely with the helper scripts below.

## Run all frameworks sequentially

Use the root-level script to execute `make run-attack` in each framework in order:

```bash
./run_all.sh
```

The script continues on failure and prints the result for each framework. It is intended as a non-regression check, not a full compatibility guarantee across all environments.

## Cleanup generated artifacts

When you want a clean repository state, run:

```bash
./cleanup_generated.sh
```

This removes generated binaries and temporary model outputs from all framework subdirectories while preserving source models and `.gitkeep` files.

## Known limitations

- The toolkit uses **AES-256-ECB** for simplicity and consistency. That choice is not the security claim: the attack is not about breaking AES or the cipher mode.
- The real demonstration is about **runtime extraction after decryption**, not about cryptanalysis of the encrypted blob.
- A stronger cipher mode or authenticated encryption would not prevent this class of attack if the decrypted model still reaches the framework runtime in plaintext.
- The toolkit assumes local code execution or the ability to inject `LD_PRELOAD`/hook libraries into the target process.
- Some frameworks require extra platform-specific setup:
  - QNN requires Python 3.10 and the QAIRT SDK, plus the GPU backend for context binary generation.
  - TensorRT requires CUDA/TensorRT headers and runtime.
  - OpenVINO requires the OpenVINO runtime and its library path configuration.

## References

- [TensorRT](https://docs.nvidia.com/deeplearning/tensorrt/)
- [OpenVINO](https://docs.openvino.ai/)
- [ONNX Runtime](https://onnxruntime.ai/docs/)
- [TensorFlow Lite](https://www.tensorflow.org/lite)
- [PyTorch C++ / LibTorch](https://pytorch.org/cppdocs/)
- [Qualcomm QNN / QAIRT SDK](https://www.qualcomm.com/developer/software/qualcomm-ai-engine-direct-sdk)

---

