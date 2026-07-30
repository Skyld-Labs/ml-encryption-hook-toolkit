#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FRAMEWORKS=(onnx openvino pytorch qnn tensorrt tflite)
FAILURES=0
for fw in "${FRAMEWORKS[@]}"; do
  echo
  echo "=== $fw ==="
  if [ ! -d "$ROOT/$fw" ]; then
    echo "SKIP: directory not found: $fw"
    FAILURES=$((FAILURES + 1))
    continue
  fi
  pushd "$ROOT/$fw" >/dev/null
  if make run-attack; then
    echo "OK: $fw"
  else
    echo "FAIL: $fw"
    FAILURES=$((FAILURES + 1))
  fi
  popd >/dev/null
done

echo
if [ "$FAILURES" -ne 0 ]; then
  echo "Completed with ${FAILURES} failures."
  exit 1
fi

echo "Completed successfully."
