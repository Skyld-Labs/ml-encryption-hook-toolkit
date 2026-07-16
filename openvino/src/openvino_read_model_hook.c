// openvino_read_model_hook.cpp - LD_PRELOAD hook intercepting ov::Core::read_model(string, Tensor).
// Dumps the decrypted IR (XML text + weights buffer) to disk before OpenVINO consumes it.

#include <openvino/openvino.hpp>
#include <dlfcn.h>
#include <cstdio>
#include <memory>

using read_model_fn = std::shared_ptr<ov::Model> (*)(const ov::Core*, const std::string&, const ov::Tensor&);
static read_model_fn g_orig = nullptr;

// __asm__("...") forces this function to bind to the exact mangled symbol name of the real
// ov::Core::read_model(const std::string&, const ov::Tensor&) const method, found via `nm -D`.
// This lets us write normal, readable C++ (real types, real ABI) instead of manual memory hacking.
extern "C" std::shared_ptr<ov::Model> hook_read_model(const ov::Core* self, const std::string& model, const ov::Tensor& weights)
    __asm__("_ZNK2ov4Core10read_modelERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEERKNS_6TensorE");

std::shared_ptr<ov::Model> hook_read_model(const ov::Core* self, const std::string& model, const ov::Tensor& weights) {
    if (!g_orig) {
        g_orig = (read_model_fn)dlsym(RTLD_NEXT,
            "_ZNK2ov4Core10read_modelERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEERKNS_6TensorE");
    }

    fprintf(stderr, "[HOOK] ov::Core::read_model(string, Tensor) intercepted\n");

    FILE* fxml = fopen("../attack/dumped_model.xml", "wb");
    if (fxml) {
        fwrite(model.data(), 1, model.size(), fxml);
        fclose(fxml);
        fprintf(stderr, "[HOOK] IR XML dumped (%zu bytes)\n", model.size());
    }

    FILE* fbin = fopen("../attack/dumped_weights.bin", "wb");
    if (fbin) {
        fwrite(weights.data(), 1, weights.get_byte_size(), fbin);
        fclose(fbin);
        fprintf(stderr, "[HOOK] weights dumped (%zu bytes)\n", weights.get_byte_size());
    }

    return g_orig(self, model, weights);
}