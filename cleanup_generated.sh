#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FRAMEWORKS=(onnx openvino pytorch qnn tensorrt tflite)
for fw in "${FRAMEWORKS[@]}"; do
  echo "Cleaning generated artifacts for: $fw"
  rm -f "$ROOT/$fw/models"/*.enc
  rm -f "$ROOT/$fw/models"/dumped_*
  rm -f "$ROOT/$fw/models/model_patched.onnx"
  find "$ROOT/$fw/build" -mindepth 1 -maxdepth 1 ! -name '.gitkeep' -exec rm -rf {} + || true
  find "$ROOT/$fw/models" -mindepth 1 -maxdepth 1 ! -name '.gitkeep' \( -name '*.enc' -o -name 'dumped_*' -o -name 'model_patched.onnx' \) -exec rm -f {} + || true
done

echo "Cleanup complete. Generated build and model artifacts removed."
