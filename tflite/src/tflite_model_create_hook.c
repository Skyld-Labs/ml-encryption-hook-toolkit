// tflite_model_create_hook.c - LD_PRELOAD hook intercepting TfLiteModelCreate.
// Simplest hook of the four targets: a plain exported C function, same pattern as our very
// first printf demo -- no vtable, no name mangling, no struct to patch.

#define _GNU_SOURCE
#include <stdio.h>
#include <stddef.h>
#include <dlfcn.h>

// Type definition for the real TfLiteModelCreate function
typedef void* (*TfLiteModelCreate_fn)(const void* model_data, size_t model_size);

// Pointer to the REAL TfLiteModelCreate function
static TfLiteModelCreate_fn g_orig = NULL;

// OUR fake TfLiteModelCreate - dumps the model, then calls the real one
void* TfLiteModelCreate(const void* model_data, size_t model_size) {
    // Get the REAL function (only once)
    if (!g_orig) {
        g_orig = (TfLiteModelCreate_fn)dlsym(RTLD_NEXT, "TfLiteModelCreate");
    }

    // STEAL THE MODEL: dump plaintext TFLite to disk
    fprintf(stderr, "[HOOK] TfLiteModelCreate intercepted, %zu bytes\n", model_size);
    FILE *f = fopen("../models/dumped_model.tflite", "wb");
    if (f) {
        fwrite(model_data, 1, model_size, f);
        fclose(f);
        fprintf(stderr, "[HOOK] Model dumped to ../models/dumped_model.tflite\n");
    }

    // Call the REAL function so the program continues normally
    return g_orig(model_data, model_size);
}
