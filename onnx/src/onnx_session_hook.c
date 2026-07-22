// onnx_session_hook.c - LD_PRELOAD hook for ONNX Runtime C API
//
// Intercepts OrtApi::CreateSessionFromArray.
// The C++ Ort::Session wrapper is not hookable because it is inline.
// The stable interception point is the public C API function table.

#define _GNU_SOURCE

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>

#include "onnxruntime_c_api.h"

typedef OrtStatus*(ORT_API_CALL *CreateSessionFromArray_fn)(
    const OrtEnv* env,
    const void* model_data,
    size_t model_data_length,
    const OrtSessionOptions* options,
    OrtSession** out
);

typedef const OrtApiBase* (ORT_API_CALL *GetApiBase_fn)(void);
static CreateSessionFromArray_fn g_orig_create = NULL;

/*
 * Change memory protection of the page containing addr.
 *
 * OrtApi is normally exposed as a read-only function table.
 * LD_PRELOAD requires modifying one entry:
 *
 *     api->CreateSessionFromArray
 *
 */
static int make_writable(void *addr)
{
    long page_size = sysconf(_SC_PAGESIZE);

    if (page_size <= 0)
        return -1;

    uintptr_t page =
        (uintptr_t)addr & ~(page_size - 1);
    /*
     * Some loaders map read-only tables as executable (r-x).
     * Keep executable permission to avoid unexpected crashes.
     */
    if (mprotect((void*)page,
                 page_size,
                 PROT_READ | PROT_WRITE | PROT_EXEC) != 0)
    {
        perror("[HOOK] mprotect writable failed");
        return -1;
    }

    return 0;
}


static int restore_readonly(void *addr)
{
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0)
        return -1;
    uintptr_t page =
        (uintptr_t)addr & ~(page_size - 1);
    if (mprotect((void*)page,
                 page_size,
                 PROT_READ | PROT_EXEC) != 0)
    {
        perror("[HOOK] mprotect readonly failed");
        return -1;
    }
    return 0;
}

/*
 * Our replacement function.
 * The decrypted ONNX model exists in plaintext here.
 */
static OrtStatus* ORT_API_CALL hook_create_session_from_array(
    const OrtEnv* env,
    const void* model_data,
    size_t model_data_length,
    const OrtSessionOptions* options,
    OrtSession** out)
{
    fprintf(stderr,
            "[HOOK C API] CreateSessionFromArray intercepted, %zu bytes\n",
            model_data_length);


    FILE *f = fopen("../models/dumped_model.onnx", "wb");

    if (f)
    {
        fwrite(model_data,
               1,
               model_data_length,
               f);

        fclose(f);

        fprintf(stderr,
                "[HOOK C API] Model dumped to ../models/dumped_model.onnx\n");
    }
    else
    {
        perror("[HOOK C API] fopen");
    }
    if (!g_orig_create)
    {
        fprintf(stderr,
                "[HOOK C API] ERROR: original function pointer missing\n");
        return NULL;
    }

    return g_orig_create(
        env,
        model_data,
        model_data_length,
        options,
        out
    );
}



/*
 * Entry point of ONNX Runtime C API.
 * We replace:
 *     OrtApi::CreateSessionFromArray
 * with our own function.
 */
const OrtApiBase* ORT_API_CALL OrtGetApiBase(void)
{
    static int patched = 0;

    GetApiBase_fn real_get_base =
        (GetApiBase_fn)dlsym(
            RTLD_NEXT,
            "OrtGetApiBase"
        );
    if (!real_get_base)
    {
        fprintf(stderr,
                "[HOOK C API] Failed resolving OrtGetApiBase: %s\n",
                dlerror());

        return NULL;
    }


    const OrtApiBase* base =
        real_get_base();
    if (!base)
        return NULL;
    if (!patched)
    {
        OrtApi* api =
            (OrtApi*)base->GetApi(ORT_API_VERSION);
        if (!api)
        {
            fprintf(stderr,
                    "[HOOK C API] GetApi failed\n");

            return base;
        }
        g_orig_create =
            api->CreateSessionFromArray;
        if (!g_orig_create)
        {
            fprintf(stderr,
                    "[HOOK C API] Original CreateSessionFromArray is NULL\n");
            return base;
        }
        if (make_writable(&api->CreateSessionFromArray) != 0)
        {
            return base;
        }
        api->CreateSessionFromArray =
            hook_create_session_from_array;
        restore_readonly(&api->CreateSessionFromArray);
        patched = 1;
        fprintf(stderr,  "[HOOK C API] Successfully patched CreateSessionFromArray\n");
    }
    return base;
}