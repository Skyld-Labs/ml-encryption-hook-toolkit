// onnx_session_hook_cxx.cpp - Experimental ONNX Runtime C++ API hook attempt.
//
// IMPORTANT:
// This file is intentionally kept as a documented dead end.
//
// The working ONNX Runtime hook is the C API hook:
//     onnx_session_hook.c
//
// Ort::Session constructors are defined inline in
// onnxruntime_cxx_api.h. Their implementation is compiled directly into
// the application binary (infer_onnx), not exported as dynamic symbols from
// libonnxruntime.so.
//
// Therefore LD_PRELOAD cannot reliably intercept Ort::Session constructors.
// The real interception point is the underlying C API:
//     OrtApi::CreateSessionFromArray
//
// This file is kept to document the investigation path and the reason why
// the C API approach is the correct attack surface.

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <dlfcn.h>
#include <cstring>
#include <vector>
#include "onnxruntime_cxx_api.h"


// Type definition for a hypothetical Session constructor symbol.
//
// In practice this symbol is not exported by libonnxruntime.so because
// Ort::Session constructors are inline C++ wrappers.
typedef void (*SessionCtor_t)(
    Ort::Session* session,
    Ort::Env& env,
    const void* model_data,
    size_t model_data_length,
    Ort::SessionOptions& options
);


// Pointer to the real constructor if a symbol ever exists.
static SessionCtor_t g_orig_ctor = NULL;



// Attempt to locate a Session constructor symbol.
//
// This normally fails because Ort::Session constructors are inline and are
// compiled into the caller binary instead of being dynamically resolved.
static SessionCtor_t find_session_constructor()
{
    const char* symbols[] = {

        // Possible GCC/Clang mangled constructor names.
        // Usually absent from libonnxruntime.so.
        "_ZN3Ort7SessionC1ERNS_3EnvEPKvmRNS_14SessionOptionsE",
        "_ZN3Ort7SessionC1ERNS_3EnvEPKvmRKNS_14SessionOptionsE",
        "_ZN3Ort7SessionC2ERNS_3EnvEPKvmRNS_14SessionOptionsE",
        "_ZN3Ort7SessionC2ERNS_3EnvEPKvmRKNS_14SessionOptionsE",

        // Other possible ABI variants.
        "_ZN3Ort7SessionC1ERNS_3EnvEPKvmRNS_14SessionOptionsERKNS_17CustomOpDomainMapE",
        "_ZN3Ort7SessionC1ERNS_3EnvEPKvmRNS_14SessionOptionsES5_",

        NULL
    };


    for (int i = 0; symbols[i] != NULL; i++)
    {
        void* sym = dlsym(RTLD_NEXT, symbols[i]);

        if (sym)
        {
            fprintf(stderr,
                    "[HOOK C++] Found unexpected symbol: %s\n",
                    symbols[i]);

            return (SessionCtor_t)sym;
        }
    }


    return NULL;
}



// This function would dump the model before calling the real constructor.
//
// It is kept for completeness, but is normally unreachable because the
// constructor symbol does not exist dynamically.
static void hook_session_ctor(
    Ort::Session* session,
    Ort::Env& env,
    const void* model_data,
    size_t model_data_length,
    Ort::SessionOptions& options)
{
    fprintf(stderr,
            "[HOOK C++] Session constructor intercepted, %zu bytes\n",
            model_data_length);


    FILE *f = fopen("../models/dumped_model_cxx.onnx", "wb");

    if (f)
    {
        fwrite(model_data,
               1,
               model_data_length,
               f);

        fclose(f);

        fprintf(stderr,
                "[HOOK C++] Model dumped to ../models/dumped_model_cxx.onnx\n");
    }
    else
    {
        fprintf(stderr,
                "[HOOK C++] Failed to dump model\n");
    }



    if (g_orig_ctor)
    {
        g_orig_ctor(
            session,
            env,
            model_data,
            model_data_length,
            options
        );
    }
    else
    {
        fprintf(stderr,
                "[HOOK C++] No original constructor available\n");
    }
}



// Initialize experimental hook.
__attribute__((constructor))
static void init_hook()
{
    fprintf(stderr,
            "[HOOK C++] Initializing experimental C++ hook\n");


    g_orig_ctor = find_session_constructor();


    if (g_orig_ctor)
    {
        fprintf(stderr,
                "[HOOK C++] Unexpectedly found Session constructor symbol\n");
    }
    else
    {
        fprintf(stderr,
                "[HOOK C++] Session constructor symbol not found\n");

        fprintf(stderr,
                "[HOOK C++] This is expected: Ort::Session is an inline wrapper\n");

        fprintf(stderr,
                "[HOOK C++] Use the C API hook instead:\n");

        fprintf(stderr,
                "[HOOK C++] LD_PRELOAD=onnx_session_hook.so\n");
    }
}


/*
 * Conclusion:
 *
 * The C++ API cannot be hooked with LD_PRELOAD in the same way as the C API.
 *
 * The working path is:
 *
 *     Ort::Session()
 *          |
 *          v
 *     OrtApi::CreateSessionFromArray()
 *          |
 *          v
 *     libonnxruntime.so
 *
 * The C API function table is dynamically dispatched and can be patched.
 * The C++ wrapper is compiled into the application and cannot be intercepted
 * through dynamic symbol replacement.
 */