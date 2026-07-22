CXX = g++
CC  = gcc

TRT_INC = -I/usr/include/aarch64-linux-gnu -I/usr/local/cuda-12.6/targets/aarch64-linux/include
TRT_LIB = -L/usr/lib/aarch64-linux-gnu -L/usr/local/cuda-12.6/targets/aarch64-linux/lib

PASSWORD ?= motdepasse123
MODEL    ?= ../models/model.onnx

BUILD_DIR = ../build

.PHONY: all clean encrypt run-baseline run-attack

all: $(BUILD_DIR)/compile $(BUILD_DIR)/infer $(BUILD_DIR)/trt_deserialize_hook.so

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/compile: compile.cpp | $(BUILD_DIR)
	$(CXX) compile.cpp -o $(BUILD_DIR)/compile $(TRT_INC) $(TRT_LIB) -lnvinfer -lnvonnxparser -lcudart -lcrypto

$(BUILD_DIR)/infer: infer.cpp | $(BUILD_DIR)
	$(CXX) -std=c++17 infer.cpp -o $(BUILD_DIR)/infer $(TRT_INC) $(TRT_LIB) -lnvinfer -lcudart -lcrypto

$(BUILD_DIR)/trt_deserialize_hook.so: trt_deserialize_hook.c | $(BUILD_DIR)
	$(CC) -shared -fPIC trt_deserialize_hook.c -o $(BUILD_DIR)/trt_deserialize_hook.so -I/usr/include/aarch64-linux-gnu -ldl

encrypt: $(BUILD_DIR)/compile
	mkdir -p ../models
	$(BUILD_DIR)/compile $(MODEL) model.engine.enc $(PASSWORD)

run-baseline: $(BUILD_DIR)/infer encrypt
	$(BUILD_DIR)/infer ../models/model.engine.enc $(PASSWORD)

run-attack: $(BUILD_DIR)/infer $(BUILD_DIR)/trt_deserialize_hook.so encrypt
	LD_PRELOAD=$(BUILD_DIR)/trt_deserialize_hook.so $(BUILD_DIR)/infer ../models/model.engine.enc $(PASSWORD)

clean:
	rm -rf $(BUILD_DIR)
	