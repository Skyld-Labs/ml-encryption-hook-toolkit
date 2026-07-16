#define _GNU_SOURCE
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>

#define IDX_DESERIALIZE 3

typedef void *(*Deserialize_ptr)(void *self, const void *blob, size_t size);
typedef void *(*create_runtime_fn_t)(void *logger, int version);

static Deserialize_ptr g_orig = NULL;

static void *hook(void *self, const void *blob, size_t size)
{
    printf("%s","[HOOK] ENGINE HOOKED \n");
    FILE *f = fopen("../attack/dumped.engine", "wb");
    if (f) {
        fwrite(blob, 1, size, f);
        fclose(f);
        printf("%s","Copy engine done \n");
    }

    return g_orig(self, blob, size);
}

static void make_writable(void *addr) {
    size_t p = sysconf(_SC_PAGESIZE);
    mprotect((void *)((uintptr_t)addr & ~(p-1)), p, PROT_READ | PROT_WRITE);
}

static void make_readonly(void *addr) {
    size_t p = sysconf(_SC_PAGESIZE);
    mprotect((void *)((uintptr_t)addr & ~(p-1)), p, PROT_READ);
}

void *createInferRuntime_INTERNAL(void *logger, int version)
{
    void *handle = dlopen("libnvinfer.so", RTLD_LAZY);
    create_runtime_fn_t real_create = (create_runtime_fn_t)dlsym(handle, "createInferRuntime_INTERNAL");

    void *runtime        = real_create(logger, version);
    void **runtime_ptr   = (void **)runtime;
    void *mImpl          = runtime_ptr[1];
    void **vtable_mImpl  = *(void ***)mImpl;

    g_orig = (Deserialize_ptr)vtable_mImpl[IDX_DESERIALIZE];
    make_writable(&vtable_mImpl[IDX_DESERIALIZE]);
    vtable_mImpl[IDX_DESERIALIZE] = (void *)hook;
    make_readonly(&vtable_mImpl[IDX_DESERIALIZE]);

    return runtime;
}