// onnx_session_hook_cxx.cpp - LD_PRELOAD hook for ONNX Runtime C++ API
// NOTE: This is optional - the C API hook alone is usually sufficient.
// The C++ hook is more fragile due to name mangling differences between compilers/versions.

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <dlfcn.h>
#include <cstring>
#include <vector>
#include "onnxruntime_cxx_api.h"

// Type definition for the real Session constructor
typedef void (*SessionCtor_t)(
    Ort::Session* session,
    Ort::Env& env,
    const void* model_data,
    size_t model_data_length,
    Ort::SessionOptions& options);

// Global pointer to the REAL constructor
static SessionCtor_t g_orig_ctor = NULL;

// Function to automatically find the correct symbol
// The symbol name depends on the compiler version and ONNX Runtime version
static SessionCtor_t find_session_constructor() {
    // List of possible symbols for different compilers/versions
    // GCC 9+ / Clang use these mangled names
    const char* symbols[] = {
        // Standard constructors (GCC 9+ / Clang)
        "_ZN3Ort7SessionC1ERNS_3EnvEPKvmRNS_14SessionOptionsE",
        "_ZN3Ort7SessionC1ERNS_3EnvEPKvmRKNS_14SessionOptionsE",
        "_ZN3Ort7SessionC2ERNS_3EnvEPKvmRNS_14SessionOptionsE",
        "_ZN3Ort7SessionC2ERNS_3EnvEPKvmRKNS_14SessionOptionsE",
        // Other possible variants (different signatures)
        "_ZN3Ort7SessionC1ERNS_3EnvEPKvmRNS_14SessionOptionsERKNS_17CustomOpDomainMapE",
        "_ZN3Ort7SessionC1ERNS_3EnvEPKvmRNS_14SessionOptionsES5_",
        NULL
    };
    
    for (int i = 0; symbols[i] != NULL; i++) {
        void* sym = dlsym(RTLD_NEXT, symbols[i]);
        if (sym) {
            fprintf(stderr, "[HOOK C++] Found symbol: %s\n", symbols[i]);
            return (SessionCtor_t)sym;
        }
    }
    
    return NULL;
}

// Hook function - dumps the model then calls the real constructor
static void hook_session_ctor(
    Ort::Session* session,
    Ort::Env& env,
    const void* model_data,
    size_t model_data_length,
    Ort::SessionOptions& options)
{
    fprintf(stderr, "[HOOK C++] Session constructor intercepted, %zu bytes\n", model_data_length);
    
    // STEAL THE MODEL: dump plaintext ONNX to disk
    FILE *f = fopen("../models/dumped_model_cxx.onnx", "wb");
    if (f) {
        fwrite(model_data, 1, model_data_length, f);
        fclose(f);
        fprintf(stderr, "[HOOK C++] Model dumped to ../models/dumped_model_cxx.onnx\n");
    } else {
        fprintf(stderr, "[HOOK C++] Failed to dump model\n");
    }
    
    // Call the REAL constructor
    if (g_orig_ctor) {
        g_orig_ctor(session, env, model_data, model_data_length, options);
    } else {
        fprintf(stderr, "[HOOK C++] ERROR: No original constructor found!\n");
    }
}

// Constructor attribute - initializes the hook when the library is loaded
__attribute__((constructor))
static void init_hook() {
    fprintf(stderr, "[HOOK C++] Initializing C++ hook\n");
    g_orig_ctor = find_session_constructor();
    
    if (g_orig_ctor) {
        fprintf(stderr, "[HOOK C++] Successfully found Session constructor\n");
    } else {
        fprintf(stderr, "[HOOK C++] WARNING: Could not find Session constructor symbol\n");
        fprintf(stderr, "[HOOK C++] Try running: nm -D /opt/onnxruntime/lib/libonnxruntime.so | grep Session | grep C1\n");
        fprintf(stderr, "[HOOK C++] Note: The C API hook is more reliable and recommended\n");
    }
}

// NOTE: Full symbol interception requires more advanced techniques
// such as using the linker's --wrap flag or using a separate library
// that overrides the specific symbol.
// 
// This hook is provided as an optional demonstration. The C API hook
// (onnx_session_hook.c) is recommended for production use because:
// 1. The C API is stable and documented
// 2. No name mangling to resolve
// 3. The C++ wrapper internally calls the C API
// 4. More reliable across ONNX Runtime versions
//
// To find the correct symbol for your specific setup:
// $ nm -D /opt/onnxruntime/lib/libonnxruntime.so | grep Session | grep C1
// $ objdump -T /opt/onnxruntime/lib/libonnxruntime.so | grep Session | grep C1
//
// If this hook doesn't work, use the C API hook instead:
// $ LD_PRELOAD=../build/onnx_session_hook.so ./infer_onnx