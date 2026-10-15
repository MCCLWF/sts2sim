// Compile against the Steam sysroot, then run inside sniper with the packaged library.
// No Python or C++ runtime is needed by this loader probe.
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>

int main(int argc, char **argv) {
    if (argc != 3) { fprintf(stderr, "usage: native_smoke LIBRARY POLICY_BUNDLE\n"); return 2; }
    void *lib = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!lib) { fprintf(stderr, "dlopen: %s\n", dlerror()); return 1; }
    void *(*create)(const char *, int32_t, uint64_t, int32_t) = dlsym(lib, "nosl_actor_create");
    void (*destroy)(void *) = dlsym(lib, "nosl_actor_destroy");
    const char *(*error)(void) = dlsym(lib, "nosl_pilot_last_error");
    if (!create || !destroy || !error || !dlsym(lib, "nosl_actor_decide")) {
        fprintf(stderr, "missing actor C ABI\n"); return 1;
    }
    // Exercise C++ exception handling through the C ABI with the old runtime loaded.
    void *bad = create("/nosl-smoke-missing-model/policy.bundle", 0, 1, 1);
    if (bad || !error() || !*error()) { fprintf(stderr, "missing-model handling failed\n"); return 1; }
    void *actor = create(argv[2], 0, 1, 1);
    if (!actor) { fprintf(stderr, "actor_create: %s\n", error()); return 1; }
    destroy(actor);
    dlclose(lib);
    puts("PASS: RTLD_NOW, actor C ABI, missing-model exception, R5 model load and destroy");
    return 0;
}
