#include <time.h>
#include "dm.h"
#include "hle_mods_gen.h"

#define F_AUTOSAVINGUI_WAIT_FOR_OPENING_LOADING 0x58
#define WAIT_LOG_EVERY 600u
#define FAIL_LOG_MAX 32u

static int mod_enabled(void) {
    static int on = -1;
    if (on < 0) {
        const char *v = getenv("DM_MOD_BOOTTRACE");
        on = !(v && v[0] == '0');
        LOG("[mod boot-trace] %s (DM_MOD_BOOTTRACE=%s)", on ? "on" : "off", v ? v : "unset");
    }
    return on;
}

static uint64_t now_ms(void) {
    static uint64_t t0;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t t = (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
    if (!t0) t0 = t;
    return t - t0;
}

static void on_wait_addressables(uint64_t *r, int p) {
    (void)r; (void)p;
    if (!mod_enabled()) return;
    static int logged;
    if (!logged++) LOG("[mod boot-trace] +%llu ms: AddressableManager initialized", (unsigned long long)now_ms());
}

static void on_wait_global(uint64_t *r, int p) {
    (void)r; (void)p;
    if (!mod_enabled()) return;
    static int logged;
    if (!logged++) LOG("[mod boot-trace] +%llu ms: GlobalGameManager initialized", (unsigned long long)now_ms());
}

static void on_wait_done(uint64_t *r, int p) {
    (void)r; (void)p;
    if (!mod_enabled()) return;
    static int logged;
    if (!logged++) LOG("[mod boot-trace] +%llu ms: startup waits done, opening load starts", (unsigned long long)now_ms());
}

static void on_load_status(uint64_t *r, int p) {
    (void)p;
    if (!mod_enabled()) return;
    uint32_t status = (uint32_t)r[0];
    static unsigned fails, oks;
    if (status == 1) {
        if (oks++ < 3) LOG("[mod boot-trace] +%llu ms: addressables load succeeded (%u so far)", (unsigned long long)now_ms(), oks);
        return;
    }
    if (fails++ < FAIL_LOG_MAX)
        LOG("[mod boot-trace] +%llu ms: addressables load ended with status %u, the game drops it and never ends its loading (%u so far)", (unsigned long long)now_ms(), status, fails);
}

static void on_opening_wait(uint64_t *r, int p) {
    (void)p;
    if (!mod_enabled()) return;
    const uint8_t *ui = (const uint8_t *)(uintptr_t)r[19];
    if (!ui) return;
    static unsigned frames;
    static int waited;
    if (ui[F_AUTOSAVINGUI_WAIT_FOR_OPENING_LOADING]) {
        if (frames % WAIT_LOG_EVERY == 0)
            LOG("[mod boot-trace] +%llu ms: auto-save notice is waiting for the opening to load, %u frames so far", (unsigned long long)now_ms(), frames);
        frames++;
        waited = 1;
        return;
    }
    if (waited) {
        waited = 0;
        LOG("[mod boot-trace] +%llu ms: opening load ended after %u frames of waiting", (unsigned long long)now_ms(), frames);
        frames = 0;
    }
}

__attribute__((constructor)) static void mod_boot_trace_attach(void) {
    gate_obs_set(MODGATE_BOOT_TRACE_WAIT_ADDRESSABLES, on_wait_addressables);
    gate_obs_set(MODGATE_BOOT_TRACE_WAIT_GLOBAL, on_wait_global);
    gate_obs_set(MODGATE_BOOT_TRACE_WAIT_DONE, on_wait_done);
    gate_obs_set(MODGATE_BOOT_TRACE_LOAD_STATUS, on_load_status);
    gate_obs_set(MODGATE_BOOT_TRACE_OPENING_WAIT, on_opening_wait);
}
