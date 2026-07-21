CXX = g++

OV_ROOT    = /opt/intel/openvino_2024.6.0
OV_INC     = -I$(OV_ROOT)/runtime/include
OV_LIB     = -L$(OV_ROOT)/runtime/lib/intel64 -L$(OV_ROOT)/runtime/3rdparty/tbb/lib
OV_RPATH   = -Wl,-rpath,$(OV_ROOT)/runtime/lib/intel64 -Wl,-rpath,$(OV_ROOT)/runtime/3rdparty/tbb/lib
OV_LDLIBS  = -lopenvino -ltbb -ltbbmalloc
OV_LDPATH  = $(OV_ROOT)/runtime/lib/intel64:$(OV_ROOT)/runtime/3rdparty/tbb/lib
OV_LIBPATH = $(OV_ROOT)/runtime/lib/intel64/libopenvino.so

# Resolve the exact mangled symbol for ov::Core::read_model(const std::string&, const ov::Tensor&) const
# at Makefile-parse time, so we never have to hardcode a version-specific name by hand.
OV_READ_MODEL_SYMBOL := $(shell nm -D $(OV_LIBPATH) 2>/dev/null | grep 'read_modelERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEERKNS_6TensorE' | awk '{print $$3}')

PASSWORD ?= motdepasse123
MODEL    ?= ../models/model.onnx

BUILD_DIR = ../build

.PHONY: all clean encrypt run-baseline run-attack check-symbol

all: check-symbol $(BUILD_DIR)/test_load $(BUILD_DIR)/compile_ov $(BUILD_DIR)/infer_ov $(BUILD_DIR)/openvino_read_model_hook.so

check-symbol:
ifeq ($(OV_READ_MODEL_SYMBOL),)
	$(error Could not resolve the mangled read_model(string, Tensor) symbol in $(OV_LIBPATH) -- OpenVINO ABI may have changed, or OV_ROOT is wrong)
endif
	@echo "Resolved read_model symbol: $(OV_READ_MODEL_SYMBOL)"

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/test_load: test_load.cpp | $(BUILD_DIR)
	$(CXX) -std=c++17 test_load.cpp -o $(BUILD_DIR)/test_load $(OV_INC) $(OV_LIB) $(OV_RPATH) $(OV_LDLIBS)

$(BUILD_DIR)/compile_ov: compile_ov.cpp | $(BUILD_DIR)
	$(CXX) -std=c++17 compile_ov.cpp -o $(BUILD_DIR)/compile_ov $(OV_INC) $(OV_LIB) $(OV_RPATH) $(OV_LDLIBS) -lcrypto

$(BUILD_DIR)/infer_ov: infer_ov.cpp | $(BUILD_DIR)
	$(CXX) -std=c++17 infer_ov.cpp -o $(BUILD_DIR)/infer_ov $(OV_INC) $(OV_LIB) $(OV_RPATH) $(OV_LDLIBS) -lcrypto

$(BUILD_DIR)/openvino_read_model_hook.so: openvino_read_model_hook.cpp check-symbol | $(BUILD_DIR)
	$(CXX) -std=c++17 -shared -fPIC openvino_read_model_hook.cpp -o $(BUILD_DIR)/openvino_read_model_hook.so \
	    -DOV_READ_MODEL_SYMBOL='"$(OV_READ_MODEL_SYMBOL)"' \
	    $(OV_INC) $(OV_LIB) $(OV_RPATH) $(OV_LDLIBS) -ldl

encrypt: $(BUILD_DIR)/compile_ov
	mkdir -p ../models
	LD_LIBRARY_PATH=$(OV_LDPATH):$$LD_LIBRARY_PATH $(BUILD_DIR)/compile_ov $(MODEL) model.ov.enc $(PASSWORD)

run-baseline: $(BUILD_DIR)/infer_ov encrypt
	LD_LIBRARY_PATH=$(OV_LDPATH):$$LD_LIBRARY_PATH $(BUILD_DIR)/infer_ov ../models/model.ov.enc $(PASSWORD)

run-attack: $(BUILD_DIR)/infer_ov $(BUILD_DIR)/openvino_read_model_hook.so encrypt
	LD_LIBRARY_PATH=$(OV_LDPATH):$$LD_LIBRARY_PATH LD_PRELOAD=$(BUILD_DIR)/openvino_read_model_hook.so $(BUILD_DIR)/infer_ov ../models/model.ov.enc $(PASSWORD)

clean:
	rm -rf $(BUILD_DIR)