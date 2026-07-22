# Makefile for ONNX Runtime hooks
CXX = g++
CC  = gcc

ORT_ROOT  = /opt/onnxruntime
ORT_INC   = -I$(ORT_ROOT)/include
ORT_LIB   = -L$(ORT_ROOT)/lib
ORT_RPATH = -Wl,-rpath,$(ORT_ROOT)/lib

PASSWORD ?= motdepasse123
MODEL    ?= ../models/model.onnx

BUILD_DIR = ../build
MODELS_DIR = ../models

.PHONY: all clean encrypt run-baseline run-attack setup

all: compile_onnx infer_onnx onnx_session_hook.so

# Not part of `all` on purpose — see README "Why the C++ API can't be hooked the same way"
hook-cxx-experimental: onnx_session_hook_cxx.cpp
	$(CXX) -shared -fPIC onnx_session_hook_cxx.cpp -o ../build/onnx_session_hook_cxx.so -I$(ORT_ROOT)/include -ldl -std=c++17

# Create directories
setup:
	mkdir -p $(BUILD_DIR) $(MODELS_DIR)

# Compile the encryptor
$(BUILD_DIR)/compile_onnx: compile_onnx.cpp | setup
	$(CXX) -std=c++17 compile_onnx.cpp -o $@ $(ORT_INC) $(ORT_LIB) $(ORT_RPATH) -lonnxruntime -lcrypto

# Compile the inference program
$(BUILD_DIR)/infer_onnx: infer_onnx.cpp | setup
	$(CXX) -std=c++17 infer_onnx.cpp -o $@ $(ORT_INC) $(ORT_LIB) $(ORT_RPATH) -lonnxruntime -lcrypto

# Compile the C API hook (recommended)
$(BUILD_DIR)/onnx_session_hook.so: onnx_session_hook.c | setup
	$(CC) -shared -fPIC onnx_session_hook.c -o $@ $(ORT_INC) -ldl

# Compile the C++ API hook (optional)
$(BUILD_DIR)/onnx_session_hook_cxx.so: onnx_session_hook_cxx.cpp | setup
	$(CXX) -shared -fPIC onnx_session_hook_cxx.cpp -o $@ $(ORT_INC) -ldl -std=c++17

encrypt: $(BUILD_DIR)/compile_onnx
	./$(BUILD_DIR)/compile_onnx $(MODEL) $(PASSWORD)

run-baseline: $(BUILD_DIR)/infer_onnx encrypt
	./$(BUILD_DIR)/infer_onnx $(MODELS_DIR)/model.onnx.enc $(PASSWORD)

# Run with C API hook only (recommended)
run-attack-c: $(BUILD_DIR)/infer_onnx $(BUILD_DIR)/onnx_session_hook.so encrypt
	LD_PRELOAD=./$(BUILD_DIR)/onnx_session_hook.so ./$(BUILD_DIR)/infer_onnx $(MODELS_DIR)/model.onnx.enc $(PASSWORD)

# Run with C++ API hook only
run-attack-cxx: $(BUILD_DIR)/infer_onnx $(BUILD_DIR)/onnx_session_hook_cxx.so encrypt
	LD_PRELOAD=./$(BUILD_DIR)/onnx_session_hook_cxx.so ./$(BUILD_DIR)/infer_onnx $(MODELS_DIR)/model.onnx.enc $(PASSWORD)

# Run with both hooks (double hook)
run-attack-double: $(BUILD_DIR)/infer_onnx $(BUILD_DIR)/onnx_session_hook.so $(BUILD_DIR)/onnx_session_hook_cxx.so encrypt
	LD_PRELOAD="./$(BUILD_DIR)/onnx_session_hook.so ./$(BUILD_DIR)/onnx_session_hook_cxx.so" \
	./$(BUILD_DIR)/infer_onnx $(MODELS_DIR)/model.onnx.enc $(PASSWORD)

# Default attack uses C API hook (recommended)
run-attack: run-attack-c

clean:
	rm -rf $(BUILD_DIR)
	rm -f $(MODELS_DIR)/*.enc
	rm -f $(MODELS_DIR)/dumped_*.onnx