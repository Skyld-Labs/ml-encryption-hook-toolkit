// openvino_read_model_hook.cpp - LD_PRELOAD hook intercepting ov::Core::read_model(string, Tensor).
// Dumps the decrypted IR (XML text + weights buffer) to disk before OpenVINO consumes it.

#include <openvino/openvino.hpp>
#include <dlfcn.h>
#include <cstdio>
#include <memory>

using read_model_fn = std::shared_ptr<ov::Model> (*)(const ov::Core*, const std::string&, const ov::Tensor&);
static read_model_fn g_orig = nullptr;

// OV_READ_MODEL_SYMBOL is injected by src/Makefile at build time (via `-DOV_READ_MODEL_SYMBOL=...`),
// which resolves the exact mangled symbol name of the real
// ov::Core::read_model(const std::string&, const ov::Tensor&) const method using `nm -D` at parse time,
// so it never has to be hardcoded by hand and stays correct across OpenVINO version upgrades.
// The fallback below is only used for manual (non-Makefile) builds and should be re-verified with:
//   nm -D <OV_ROOT>/runtime/lib/intel64/libopenvino.so | grep read_model
#ifndef OV_READ_MODEL_SYMBOL
#define OV_READ_MODEL_SYMBOL "_ZNK2ov4Core10read_modelERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEERKNS_6TensorE"
#endif

// __asm__(...) forces this function to bind to the exact mangled symbol name above.
// This lets us write normal, readable C++ (real types, real ABI) instead of manual memory hacking.
extern "C" std::shared_ptr<ov::Model> hook_read_model(const ov::Core* self, const std::string& model, const ov::Tensor& weights)
    __asm__(OV_READ_MODEL_SYMBOL);

std::shared_ptr<ov::Model> hook_read_model(const ov::Core* self, const std::string& model, const ov::Tensor& weights) {
    if (!g_orig) {
        g_orig = (read_model_fn)dlsym(RTLD_NEXT, OV_READ_MODEL_SYMBOL);
    }

    fprintf(stderr, "[HOOK] ov::Core::read_model(string, Tensor) intercepted\n");

    FILE* fxml = fopen("../models/dumped_model.xml", "wb");
    if (fxml) {
        fwrite(model.data(), 1, model.size(), fxml);
        fclose(fxml);
        fprintf(stderr, "[HOOK] IR XML dumped (%zu bytes)\n", model.size());
    }

    FILE* fbin = fopen("../models/dumped_weights.bin", "wb");
    if (fbin) {
        fwrite(weights.data(), 1, weights.get_byte_size(), fbin);
        fclose(fbin);
        fprintf(stderr, "[HOOK] weights dumped (%zu bytes)\n", weights.get_byte_size());
    }

    return g_orig(self, model, weights);
}