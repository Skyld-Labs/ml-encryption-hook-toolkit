# Makefile - PyTorch/LibTorch encryption hook demo
#
# This Makefile builds three components:
#   1. compile_pt       — Encrypts a .pt model with AES-256-ECB
#   2. infer_pt         — Decrypts and loads the encrypted model (victim program)
#   3. pt_load_hook.so  — LD_PRELOAD hook that intercepts torch::jit::load(istream&)
#
# All binaries are placed in ../build/ (git-ignored).
# All model files (original, encrypted, stolen) go in ../models/.

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -O2 -D_GLIBCXX_USE_CXX11_ABI=1

BUILD_DIR  = ../build
MODELS_DIR = ../models

PASSWORD ?= changeme
MODEL    ?= $(MODELS_DIR)/model.pt

LIBTORCH_ROOT ?= $(HOME)/Documents/Skyld/libtorch
TORCH_INC   = -I$(LIBTORCH_ROOT)/include -I$(LIBTORCH_ROOT)/include/torch/csrc/api/include
TORCH_LIB   = -L$(LIBTORCH_ROOT)/lib
TORCH_RPATH = -Wl,-rpath,$(LIBTORCH_ROOT)/lib
TORCH_LINK  = -ltorch -ltorch_cpu -lc10

.PHONY: all clean encrypt run-baseline run-attack check-env setup

check-env:
	@if [ ! -d "$(LIBTORCH_ROOT)" ]; then \
		echo "ERROR: LIBTORCH_ROOT not found at $(LIBTORCH_ROOT)"; \
		echo "Run ../install_requirements.sh first."; \
		exit 1; \
	fi

all: check-env $(BUILD_DIR)/compile_pt $(BUILD_DIR)/infer_pt $(BUILD_DIR)/pt_load_hook.so

# Encryptor: reads plaintext .pt, writes encrypted .pt.enc
$(BUILD_DIR)/compile_pt: compile_pt.cpp | setup
	$(CXX) $(CXXFLAGS) $< -o $@ -lcrypto

# Victim program: decrypts .pt.enc and loads via torch::jit::load(istream&)
$(BUILD_DIR)/infer_pt: infer_pt.cpp | setup
	$(CXX) $(CXXFLAGS) $(TORCH_INC) $< -o $@ \
		$(TORCH_LIB) $(TORCH_RPATH) $(TORCH_LINK) -lcrypto

# LD_PRELOAD hook: intercepts torch::jit::load(istream&) and dumps plaintext model
$(BUILD_DIR)/pt_load_hook.so: pt_load_hook.cpp | setup
	$(CXX) $(CXXFLAGS) -fPIC -shared $(TORCH_INC) $< -o $@ \
		$(TORCH_LIB) $(TORCH_RPATH) $(TORCH_LINK) -ldl

setup:
	mkdir -p $(BUILD_DIR) $(MODELS_DIR)

encrypt: $(BUILD_DIR)/compile_pt
	$(BUILD_DIR)/compile_pt "$(PASSWORD)" $(MODEL) $(MODELS_DIR)/model.pt.enc

run-baseline: $(BUILD_DIR)/infer_pt encrypt
	cd $(BUILD_DIR) && LD_LIBRARY_PATH=$(LIBTORCH_ROOT)/lib \
		./infer_pt ../models/model.pt.enc "$(PASSWORD)"

run-attack: $(BUILD_DIR)/infer_pt $(BUILD_DIR)/pt_load_hook.so encrypt
	cd $(BUILD_DIR) && LD_LIBRARY_PATH=$(LIBTORCH_ROOT)/lib \
		LD_PRELOAD=./pt_load_hook.so ./infer_pt ../models/model.pt.enc "$(PASSWORD)"
	@echo ""
	@echo "Comparing dumped buffer against the original plaintext model:"
	diff $(MODELS_DIR)/dumped_model.pt $(MODEL) \
		&& echo "IDENTICAL — theft confirmed"

clean:
	rm -f $(MODELS_DIR)/model.pt.enc
	rm -f $(MODELS_DIR)/dumped_model.pt
	rm -f $(BUILD_DIR)/compile_pt $(BUILD_DIR)/infer_pt $(BUILD_DIR)/pt_load_hook.so