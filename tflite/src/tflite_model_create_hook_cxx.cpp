// tflite_model_create_hook_cxx.cpp - Validated C++-level hook of TFLite's internal loader.
//
// Unlike the ONNX Runtime C++ attempt (Ort::Session's constructor is fully inlined into the
// caller -- no dynamic symbol exists at all, so it can never be hooked), TFLite's internal
// FlatBufferModelBase<FlatBufferModel>::VerifyAndBuildFromAllocation IS exported as a weak
// dynamic symbol, confirmed with:
//   nm -D libtensorflow-lite.so | grep VerifyAndBuildFromAllocation
// so it is a valid LD_PRELOAD target, and this hook has been confirmed working end-to-end:
// it intercepts the call, dumps a decrypted model byte-for-byte identical to the original,
// and inference completes normally afterward with no visible disruption.
//
// This file is NOT the toolkit's primary TFLite hook and is not built by `make all`.
// The primary hook (tflite_model_create_hook.c) targets TfLiteModelCreate, the stable public
// C API entry point: simpler, documented, and independent of TFLite's internal Allocation
// abstraction. This file is kept as a second, validated hook demonstrating that the same
// weakness is also reachable at the internal C++ level -- build and run it explicitly with
// `make hook-cxx-experimental`.
//
// Why this is harder than the C API hook: the real function does not take a flat
// (void*, size_t) buffer. It takes ownership of a std::unique_ptr<Allocation>, an internal
// TFLite abstraction over a memory region. We read it through Allocation's public base()/
// bytes() accessors (tensorflow/lite/allocation.h) before forwarding the allocation, moved
// and unconsumed, to the real implementation.

#include <dlfcn.h>
#include <cstdio>
#include <memory>
#include <tensorflow/lite/allocation.h>
#include <tensorflow/lite/core/model_builder.h>

using tflite::Allocation;
using tflite::TfLiteVerifier;
using tflite::ErrorReporter;
using tflite::impl::FlatBufferModel;

using VerifyAndBuild_fn = std::unique_ptr<FlatBufferModel> (*)(
    std::unique_ptr<Allocation>, TfLiteVerifier*, ErrorReporter*);

static VerifyAndBuild_fn g_orig = nullptr;

// Exact mangled symbol name, confirmed via `nm -D` against libtensorflow-lite.so (TF v2.18.0).
// This is version-specific -- re-verify it if TensorFlow is updated.
#define TFLITE_VERIFY_BUILD_SYMBOL \
    "_ZN6tflite4impl19FlatBufferModelBaseINS0_15FlatBufferModelEE28VerifyAndBuildFromAllocationESt10unique_ptrINS_10AllocationESt14default_deleteIS5_EEPNS_14TfLiteVerifierEPNS_13ErrorReporterE"

// __asm__(...) forces this function to bind to the exact mangled symbol above, the same
// technique used by the OpenVINO hook -- see openvino/src/openvino_read_model_hook.cpp.
extern "C" std::unique_ptr<FlatBufferModel> hook_verify_and_build(
    std::unique_ptr<Allocation> allocation, TfLiteVerifier* verifier, ErrorReporter* reporter)
    __asm__(TFLITE_VERIFY_BUILD_SYMBOL);

std::unique_ptr<FlatBufferModel> hook_verify_and_build(
    std::unique_ptr<Allocation> allocation, TfLiteVerifier* verifier, ErrorReporter* reporter) {
    if (!g_orig) {
        g_orig = (VerifyAndBuild_fn)dlsym(RTLD_NEXT, TFLITE_VERIFY_BUILD_SYMBOL);
    }

    if (allocation) {
        fprintf(stderr, "[HOOK C++] VerifyAndBuildFromAllocation intercepted, %zu bytes\n",
                allocation->bytes());
        FILE* f = fopen("../models/dumped_model_cxx.tflite", "wb");
        if (f) {
            fwrite(allocation->base(), 1, allocation->bytes(), f);
            fclose(f);
            fprintf(stderr, "[HOOK C++] Model dumped to ../models/dumped_model_cxx.tflite\n");
        }
    }

    // Forward the allocation exactly as received -- ownership must transfer via move,
    // not copy, to match the real function's signature.
    return g_orig(std::move(allocation), verifier, reporter);
}