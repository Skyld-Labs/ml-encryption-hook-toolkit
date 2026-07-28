// pt_load_hook.cpp - LD_PRELOAD hook targeting torch::jit::load(istream&).
//
// Unlike ONNX Runtime (patch a C API function-pointer table) or QNN
// (intercept dlsym to catch a table-fetching function), torch::jit::load
// is a plain exported C++ function - the same shape as TensorRT's
// deserializeCudaEngine(). The technique here is symbol interposition:
// we define our own function in the exact same namespace with the exact
// same signature, so it mangles to the identical symbol name. When this
// .so is LD_PRELOAD'd, the dynamic linker resolves calls to that symbol
// to OUR definition first, before libtorch_cpu.so's real one.
//
// Mangled target (confirmed via nm -D):
//   _ZN5torch3jit4loadERSiSt8optionalIN3c106DeviceEEb
//   == torch::jit::load(std::istream&, std::optional<c10::Device>, bool)

#define _GNU_SOURCE
#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <optional>

#include <torch/script.h>

namespace torch {
namespace jit {

using load_istream_fn = Module (*)(std::istream&, std::optional<c10::Device>, bool);

static load_istream_fn real_load = nullptr;

// This function's mangled name collides with the real torch::jit::load
// overload on purpose - that's the entire mechanism.
Module load(std::istream& in, std::optional<c10::Device> device, bool load_debug_files) {
    if (!real_load) {
        real_load = reinterpret_cast<load_istream_fn>(
            dlsym(RTLD_NEXT, "_ZN5torch3jit4loadERSiSt8optionalIN3c106DeviceEEb"));
        if (!real_load) {
            fprintf(stderr, "[HOOK] Failed to resolve real torch::jit::load: %s\n", dlerror());
            exit(1);
        }
        fprintf(stderr, "[HOOK] Resolved real torch::jit::load via dlsym(RTLD_NEXT)\n");
    }

    fprintf(stderr, "[HOOK] torch::jit::load(istream&) intercepted\n");

    // Capture the current stream position, then read the entire buffer
    // out so we can dump it - this is the decrypted model, in RAM only.
    std::streampos start = in.tellg();
    in.seekg(0, std::ios::end);
    std::streampos end = in.tellg();
    in.seekg(start);

    size_t size = static_cast<size_t>(end - start);
    std::vector<char> buffer(size);
    in.read(buffer.data(), static_cast<std::streamsize>(size));

    FILE* f = fopen("../models/dumped_model.pt", "wb");
    if (f) {
        fwrite(buffer.data(), 1, buffer.size(), f);
        fclose(f);
        fprintf(stderr,
                "[HOOK] Decrypted model dumped to ../models/dumped_model.pt (%zu bytes)\n",
                buffer.size());
    } else {
        fprintf(stderr, "[HOOK] Failed to open dump file for writing\n");
    }

    // Rewind the stream so the real load() sees the full buffer again -
    // the victim program must behave exactly as if nothing happened.
    in.clear();
    in.seekg(start);

    return real_load(in, device, load_debug_files);
}

}  // namespace jit
}  // namespace torch