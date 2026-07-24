#!/usr/bin/env python3
"""
Patch ONNX model to fix Reshape allowzero attribute.
QNN converter only supports allowzero=0 (pre-opset-14 default).
"""

import onnx
import sys


def patch_reshape_allowzero(input_path, output_path):
    """
    Patch Reshape allowzero attributes in ONNX model.
    
    Args:
        input_path: Path to input ONNX model
        output_path: Path to output patched ONNX model
    """
    print(f"Loading: {input_path}")
    model = onnx.load(input_path)
    
    patched = 0
    for node in model.graph.node:
        if node.op_type == 'Reshape':
            for attr in node.attribute:
                if attr.name == 'allowzero':
                    attr.i = 0
                    patched += 1
                    print(f"  Patched Reshape node: {node.name}")
    
    print(f"Patched {patched} Reshape node(s)")
    onnx.save(model, output_path)
    print(f"Saved: {output_path}")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: patch_onnx.py <input.onnx> <output.onnx>")
        sys.exit(1)
    patch_reshape_allowzero(sys.argv[1], sys.argv[2])