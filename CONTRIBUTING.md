# Contributing to ML Encryption Hook Toolkit

Thank you for contributing! This project demonstrates runtime model extraction attacks across multiple inference frameworks. Please follow these guidelines to keep contributions consistent and high quality.

## Proposing a new framework

1. Open an issue describing the framework and the intended attack surface.
2. Confirm that the target framework follows the same general model:
   - encrypted model artifact on disk
   - decryption in memory at runtime
   - hook point available during model loading or deserialization
3. Add a new top-level directory with at least:
   - `README.md`
   - `install_requirements.sh`
   - `src/Makefile`
   - `src/` containing the proof-of-concept source code
   - `build/.gitkeep`
   - `models/.gitkeep`
4. Follow the existing framework structure and document any framework-specific prerequisites.

## Adding a hook

1. Document the interception point clearly in the framework's `README.md`.
2. Implement the attack in `src/` using an `LD_PRELOAD`-style hook or the appropriate runtime interception technique.
3. Keep generated binaries in `build/` and generated model outputs or encrypted files out of version control.
4. Ensure temporary artifacts are not committed.

## Artifact validation

Do not commit generated artifacts such as:

- `**/build/*` except `**/build/.gitkeep`
- `**/models/*.enc`
- `**/models/dumped_*`
- `**/models/*.bin`, `**/models/*.xml`, `**/models/*.tflite`, `**/models/*.engine`, `**/models/*.onnx`
- `qnn/models/model_patched.onnx`

If you add a new framework, update the root `README.md`, `run_all.sh`, `cleanup_generated.sh`, and the root `Makefile` as appropriate.

## Preparing a clean pull request

1. Create a dedicated branch for your feature.
2. Describe the feature clearly in the PR: target framework, attack approach, and any special caveats.
3. Verify locally:
   - `make lint`
   - `make test`
   - `bash ./run_all.sh` if applicable
4. Keep README content consistent with the existing framework documentation style.
5. Ensure framework READMEs include the following sections:
   - `Overview`
   - `Structure`
   - `Requirements`
   - `Build & run`
   - `What each target does`
   - `Troubleshooting / Quirks`
   - `References`

## Questions

If you are unsure about the structure or validation requirements for a new hook, open an issue before developing. This helps avoid duplicate effort and keeps contributions aligned.
