// onnx_session_hook.c - LD_PRELOAD hook for ONNX Runtime C API
// This is the primary and recommended hook approach.
// It intercepts the stable C API: OrtApi::CreateSessionFromArray

#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>
#include "onnxruntime_c_api.h"

// Type definitions for the real functions
typedef OrtStatus*(ORT_API_CALL *CreateSessionFromArray_fn)(
    const OrtEnv* env, const void* model_data, size_t model_data_length,
    const OrtSessionOptions* options, OrtSession** out);

typedef const OrtApiBase* (ORT_API_CALL *GetApiBase_fn)(void);

// Global pointer to the REAL CreateSessionFromArray function
static CreateSessionFromArray_fn g_orig_create = NULL;

// Memory protection helpers
static void make_writable(void *addr) {
    long p = sysconf(_SC_PAGESIZE);
    mprotect((void *)((uintptr_t)addr & ~(p - 1)), p, PROT_READ | PROT_WRITE);
}

static void make_readonly(void *addr) {
    long p = sysconf(_SC_PAGESIZE);
    mprotect((void *)((uintptr_t)addr & ~(p - 1)), p, PROT_READ);
}

// Hook function - dumps the model then calls the real one
static OrtStatus* ORT_API_CALL hook_create_session_from_array(
    const OrtEnv* env, const void* model_data, size_t model_data_length,
    const OrtSessionOptions* options, OrtSession** out)
{
    fprintf(stderr, "[HOOK C API] CreateSessionFromArray intercepted, %zu bytes\n", model_data_length);
    
    // STEAL THE MODEL: dump plaintext ONNX to disk
    FILE *f = fopen("../models/dumped_model.onnx", "wb");
    if (f) {
        fwrite(model_data, 1, model_data_length, f);
        fclose(f);
        fprintf(stderr, "[HOOK C API] Model dumped to ../models/dumped_model.onnx\n");
    } else {
        fprintf(stderr, "[HOOK C API] Failed to dump model\n");
    }
    
    // Call the REAL function so the program continues normally
    return g_orig_create(env, model_data, model_data_length, options, out);
}

// Hook OrtGetApiBase - the entry point of ONNX Runtime's C API
// This is called ONCE when the program starts
const OrtApiBase* ORT_API_CALL OrtGetApiBase(void)
{
    static int patched = 0;

    // Get the REAL OrtGetApiBase function
    GetApiBase_fn real_get_base = (GetApiBase_fn)dlsym(RTLD_NEXT, "OrtGetApiBase");
    if (!real_get_base) {
        fprintf(stderr, "[HOOK C API] Failed to resolve real OrtGetApiBase: %s\n", dlerror());
        return NULL;
    }

    // Call the real function to get the API base
    const OrtApiBase *base = real_get_base();
    if (!base) return NULL;

    // Patch only once
    if (!patched) {
        // Get the OrtApi struct (table of function pointers)
        OrtApi *api = (OrtApi *)base->GetApi(ORT_API_VERSION);
        if (!api) {
            fprintf(stderr, "[HOOK C API] GetApi returned NULL (version mismatch?)\n");
            return base;
        }

        // Save the original function pointer
        g_orig_create = api->CreateSessionFromArray;
        
        // Patch: replace with our hook
        make_writable(&api->CreateSessionFromArray);
        api->CreateSessionFromArray = hook_create_session_from_array;
        make_readonly(&api->CreateSessionFromArray);
        
        patched = 1;
        fprintf(stderr, "[HOOK C API] Successfully patched CreateSessionFromArray\n");
    }

    return base;
}