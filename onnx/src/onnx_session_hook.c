// onnx_session_hook.c - LD_PRELOAD hook intercepting OrtApi::CreateSessionFromArray.
// ONNX Runtime exposes a stable C API: we patch the function pointer directly.

#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>
#include "onnxruntime_c_api.h"

// Type definition for the real CreateSessionFromArray function
typedef OrtStatus*(ORT_API_CALL *CreateSessionFromArray_fn)(
    const OrtEnv* env, const void* model_data, size_t model_data_length,
    const OrtSessionOptions* options, OrtSession** out);

// Type definition for OrtGetApiBase
typedef const OrtApiBase* (ORT_API_CALL *GetApiBase_fn)(void);

// Pointer to the REAL CreateSessionFromArray function
static CreateSessionFromArray_fn g_orig_create = NULL;

// Temporarily make memory writable to patch the function pointer
static void make_writable(void *addr) {
    long p = sysconf(_SC_PAGESIZE);
    mprotect((void *)((uintptr_t)addr & ~(p - 1)), p, PROT_READ | PROT_WRITE);
}

// Restore memory to read-only after patching
static void make_readonly(void *addr) {
    long p = sysconf(_SC_PAGESIZE);
    mprotect((void *)((uintptr_t)addr & ~(p - 1)), p, PROT_READ);
}

// OUR fake CreateSessionFromArray - dumps the model, then calls the real one
static OrtStatus* ORT_API_CALL hook_create_session_from_array(
    const OrtEnv* env, const void* model_data, size_t model_data_length,
    const OrtSessionOptions* options, OrtSession** out)
{
    fprintf(stderr, "[HOOK] CreateSessionFromArray intercepted, %zu bytes\n", model_data_length);
    
    // STEAL THE MODEL: dump plaintext ONNX to disk
    FILE *f = fopen("../attack/dumped_model.onnx", "wb");
    if (f) {
        fwrite(model_data, 1, model_data_length, f);
        fclose(f);
        fprintf(stderr, "[HOOK] Model dumped to ../attack/dumped_model.onnx\n");
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
        fprintf(stderr, "[HOOK] Failed to resolve real OrtGetApiBase: %s\n", dlerror());
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
            fprintf(stderr, "[HOOK] GetApi returned NULL (version mismatch?)\n");
            return base;
        }

        // Save the original function pointer
        g_orig_create = api->CreateSessionFromArray;
        
        // Patch: replace with our hook
        make_writable(&api->CreateSessionFromArray);
        api->CreateSessionFromArray = hook_create_session_from_array;
        make_readonly(&api->CreateSessionFromArray);
        
        patched = 1;
    }

    return base;
}
