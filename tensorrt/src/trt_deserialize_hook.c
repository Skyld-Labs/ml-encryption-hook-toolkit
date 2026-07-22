/*
 * trt_deserialize_hook.c
 *
 * Purpose:
 *  - Hook TensorRT's runtime deserialize function to intercept the serialized
 *    engine blob when an engine is deserialized. This allows dumping the
 *    in-memory engine binary to disk for analysis or debugging.
 *
 * How it works (high level):
 *  1. Resolve the real `createInferRuntime_INTERNAL` from libnvinfer at runtime.
 *  2. Call the real function to obtain a runtime object.
 *  3. Walk the runtime object's internal pointers to reach the implementation
 *     vtable and replace the deserialize entry with our `hook` function.
 *  4. The hook saves the provided `blob` to `../models/dumped_model.engine` and
 *     then delegates back to the original deserialize function.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>

/* Index in the implementation vtable corresponding to the deserialize method.
 * This index is specific to the targeted TensorRT version and was observed
 * empirically; it may need adjustment for other versions. */
#define IDX_DESERIALIZE 3

/* Function pointer types for the methods we interact with. */
typedef void *(*Deserialize_ptr)(void *self, const void *blob, size_t size);
typedef void *(*create_runtime_fn_t)(void *logger, int version);

/* Global storage for the original deserialize function so the hook can call it. */
static Deserialize_ptr g_orig = NULL;

/*
 * hook - replacement for the deserialize vtable entry.
 *  - Saves the serialized engine blob to a file (relative path) for analysis
 *  - Calls the original deserialize implementation to preserve functionality
 */
static void *hook(void *self, const void *blob, size_t size)
{
    /* Simple marker to indicate hook execution in stdout. */
    printf("%s","[HOOK] ENGINE HOOKED \n");

    /* Dump the raw engine blob to a file for offline inspection. */
    FILE *f = fopen("../models/dumped_model.engine", "wb");
    if (f) {
        fwrite(blob, 1, size, f);
        fclose(f);
        printf("%s","Copy engine done \n");
    }

    /* Delegate back to the original deserialize routine to keep behavior. */
    return g_orig(self, blob, size);
}

/*
 * Helper to set the memory page containing `addr` writable. We compute the
 * page-aligned address and call `mprotect`. This is necessary because vtables
 * are typically in read-only memory.
 */
static void make_writable(void *addr) {
    size_t p = sysconf(_SC_PAGESIZE);
    mprotect((void *)((uintptr_t)addr & ~(p-1)), p, PROT_READ | PROT_WRITE);
}

/* Restore the page to read-only after patching. */
static void make_readonly(void *addr) {
    size_t p = sysconf(_SC_PAGESIZE);
    mprotect((void *)((uintptr_t)addr & ~(p-1)), p, PROT_READ);
}

/*
 * createInferRuntime_INTERNAL - wrapper that resolves the real function from
 * libnvinfer, calls it, locates the implementation vtable and patches the
 * deserialize entry.
 *
 * Notes on object layout:
 *  - `runtime` is treated as an array of pointers; the second entry (index 1)
 *    is the implementation object (`mImpl`). From `mImpl` we dereference to
 *    obtain the pointer to its vtable (`vtable_mImpl`). This is a heuristic
 *    that matches the observed in-memory layout for the targeted TensorRT
 *    version but is not guaranteed to be stable across versions.
 */
void *createInferRuntime_INTERNAL(void *logger, int version)
{
    /* Dynamically open TensorRT library and lookup the real function. */
    void *handle = dlopen("libnvinfer.so", RTLD_LAZY);
    create_runtime_fn_t real_create = (create_runtime_fn_t)dlsym(handle, "createInferRuntime_INTERNAL");

    /* Create the real runtime object using the original function. */
    void *runtime        = real_create(logger, version);

    /* Walk the runtime object to locate the implementation vtable. */
    void **runtime_ptr   = (void **)runtime;
    void *mImpl          = runtime_ptr[1];
    void **vtable_mImpl  = *(void ***)mImpl;

    /* Save original deserialize pointer, make the vtable entry writable,
     * replace it with our hook, then restore read-only protection. */
    g_orig = (Deserialize_ptr)vtable_mImpl[IDX_DESERIALIZE];
    make_writable(&vtable_mImpl[IDX_DESERIALIZE]);
    vtable_mImpl[IDX_DESERIALIZE] = (void *)hook;
    make_readonly(&vtable_mImpl[IDX_DESERIALIZE]);

    return runtime;
}