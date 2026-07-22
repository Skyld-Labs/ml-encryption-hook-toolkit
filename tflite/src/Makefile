CXX = g++
CC  = gcc

TFLITE_SRC ?= $(HOME)/Downloads/tensorflow
TFLITE_BUILD_LIB ?= $(HOME)/Downloads/tflite_build

# Correct FlatBuffers include path
# Le fichier base.h est dans: /home/elodie/Downloads/tflite_build/flatbuffers/include/flatbuffers/base.h
FLATBUFFERS_INC  = -I$(TFLITE_BUILD_LIB)/flatbuffers/include
TFLITE_INC       = -I$(TFLITE_SRC) $(FLATBUFFERS_INC)
TFLITE_LIB       = -L$(TFLITE_BUILD_LIB)
TFLITE_RPATH     = -Wl,-rpath,$(TFLITE_BUILD_LIB)

BUILD_DIR  = ../build
MODELS_DIR = ../models

PASSWORD ?= motdepasse123
MODEL    ?= $(MODELS_DIR)/model.tflite

.PHONY: all clean encrypt run-baseline run-attack hook-cxx-experimental

all: $(BUILD_DIR)/compile_tflite $(BUILD_DIR)/infer_tflite $(BUILD_DIR)/tflite_model_create_hook.so

$(BUILD_DIR)/compile_tflite: compile_tflite.cpp
	mkdir -p $(BUILD_DIR)
	$(CXX) -std=c++17 compile_tflite.cpp -o $(BUILD_DIR)/compile_tflite -lcrypto

$(BUILD_DIR)/infer_tflite: infer_tflite.cpp
	mkdir -p $(BUILD_DIR)
	$(CXX) -std=c++17 infer_tflite.cpp -o $(BUILD_DIR)/infer_tflite $(TFLITE_INC) $(TFLITE_LIB) $(TFLITE_RPATH) -ltensorflow-lite -lcrypto

$(BUILD_DIR)/tflite_model_create_hook.so: tflite_model_create_hook.c
	mkdir -p $(BUILD_DIR)
	$(CC) -shared -fPIC tflite_model_create_hook.c -o $(BUILD_DIR)/tflite_model_create_hook.so -ldl

# Experimental C++ hook - with correct FlatBuffers path
hook-cxx-experimental: tflite_model_create_hook_cxx.cpp
	mkdir -p $(BUILD_DIR)
	$(CXX) -std=c++17 -shared -fPIC tflite_model_create_hook_cxx.cpp -o $(BUILD_DIR)/tflite_model_create_hook_cxx.so $(TFLITE_INC) $(TFLITE_LIB) $(TFLITE_RPATH) -ltensorflow-lite -ldl

encrypt: $(BUILD_DIR)/compile_tflite
	mkdir -p $(MODELS_DIR)
	$(BUILD_DIR)/compile_tflite $(MODEL) model.tflite.enc $(PASSWORD)

run-baseline: $(BUILD_DIR)/infer_tflite encrypt
	LD_LIBRARY_PATH=$(TFLITE_BUILD_LIB):$$LD_LIBRARY_PATH $(BUILD_DIR)/infer_tflite $(MODELS_DIR)/model.tflite.enc $(PASSWORD)

run-attack: $(BUILD_DIR)/infer_tflite $(BUILD_DIR)/tflite_model_create_hook.so encrypt
	LD_LIBRARY_PATH=$(TFLITE_BUILD_LIB):$$LD_LIBRARY_PATH LD_PRELOAD=$(BUILD_DIR)/tflite_model_create_hook.so $(BUILD_DIR)/infer_tflite $(MODELS_DIR)/model.tflite.enc $(PASSWORD)

# Run the experimental C++ hook manually once built:
#   LD_LIBRARY_PATH=$(TFLITE_BUILD_LIB):$$LD_LIBRARY_PATH LD_PRELOAD=$(BUILD_DIR)/tflite_model_create_hook_cxx.so $(BUILD_DIR)/infer_tflite $(MODELS_DIR)/model.tflite.enc $(PASSWORD)

clean:
	rm -rf $(BUILD_DIR)
		