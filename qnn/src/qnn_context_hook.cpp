// qnn_context_hook.cpp
// LD_PRELOAD hook: intercepts dlsym("QnnInterface_getProviders") in order to
// patch contextCreateFromBinary in the returned QNN_INTERFACE table, and
// dumps the decrypted buffer at the moment the victim program calls
// createFromBinary.

#define _GNU_SOURCE
#include <dlfcn.h>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <string> 

#include <QNN/QnnInterface.h>
#include <QNN/QnnContext.h>

// --- Real dlsym, resolved via dlvsym (avoids infinite recursion) ---
typedef void* (*dlsym_fn_t)(void*, const char*);
static dlsym_fn_t real_dlsym = nullptr;


// --- Real contextCreateFromBinary function, saved after patching ---
typedef Qnn_ErrorHandle_t (*ContextCreateFromBinary_fn)(
    Qnn_BackendHandle_t, Qnn_DeviceHandle_t, const QnnContext_Config_t**,
    const void*, Qnn_ContextBinarySize_t, Qnn_ContextHandle_t*, Qnn_ProfileHandle_t);

static ContextCreateFromBinary_fn real_contextCreateFromBinary = nullptr;

// --- Our spy function, called in place of the real one ---
static Qnn_ErrorHandle_t hooked_contextCreateFromBinary(
        Qnn_BackendHandle_t backend,
        Qnn_DeviceHandle_t device,
        const QnnContext_Config_t** config,
        const void* binaryBuffer,
        Qnn_ContextBinarySize_t binaryBufferSize,
        Qnn_ContextHandle_t* context,
        Qnn_ProfileHandle_t profile) {

    fprintf(stderr, "[HOOK] QnnContext_createFromBinary intercepted! Buffer size = %llu bytes\n",
            (unsigned long long)binaryBufferSize);

    FILE* f = fopen("../models/dumped_model_context.bin", "wb");
    if (f) {
        fwrite(binaryBuffer, 1, binaryBufferSize, f);
        fclose(f);
        fprintf(stderr, "[HOOK] Decrypted context binary dumped to ../models/dumped_model_context.bin\n");
    } else {
        fprintf(stderr, "[HOOK] Failed to open dump file for writing\n");
    }

    // Call the real function so the victim program keeps working normally
    return real_contextCreateFromBinary(backend, device, config, binaryBuffer,
                                         binaryBufferSize, context, profile);
}

// --- Type of the original QnnInterface_getProviders function ---
typedef Qnn_ErrorHandle_t (*GetProvidersFn)(const QnnInterface_t***, uint32_t*);
static GetProvidersFn real_getProviders = nullptr;

// --- Wrapper that replaces QnnInterface_getProviders ---
static Qnn_ErrorHandle_t hooked_getProviders(const QnnInterface_t*** providerList,
                                              uint32_t* numProviders) {
    Qnn_ErrorHandle_t result = real_getProviders(providerList, numProviders);

    if (result == QNN_SUCCESS && providerList && *providerList && numProviders && *numProviders > 0) {
        // Patch the first provider: save the real function, replace the
        // pointer in the table with our spy.
        // NOTE: the QnnInterface_t/QNN_INTERFACE_VER_NAME struct is not
        // actually const in memory, even though the type declares
        // 'const **' — we cast it away to patch the function pointer in place.
        QnnInterface_t* provider = const_cast<QnnInterface_t*>((*providerList)[0]);

        real_contextCreateFromBinary = provider->QNN_INTERFACE_VER_NAME.contextCreateFromBinary;
        provider->QNN_INTERFACE_VER_NAME.contextCreateFromBinary = hooked_contextCreateFromBinary;

        fprintf(stderr, "[HOOK] Patched QNN_INTERFACE.contextCreateFromBinary\n");
    }

    return result;
}

// --- Intercepted dlsym ---
extern "C" void* dlsym(void* handle, const char* symbol) {
    if (!real_dlsym) {
        real_dlsym = (dlsym_fn_t)dlvsym(RTLD_NEXT, "dlsym", "GLIBC_2.2.5");
    }

    void* real_sym = real_dlsym(handle, symbol);

    if (real_sym && strcmp(symbol, "QnnInterface_getProviders") == 0) {
        real_getProviders = (GetProvidersFn)real_sym;
        fprintf(stderr, "[HOOK] Intercepted dlsym(\"QnnInterface_getProviders\")\n");
        return (void*)hooked_getProviders;
    }

    return real_sym;
}
