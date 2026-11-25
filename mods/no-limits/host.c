#include "dm.h"
#include "hle_mods_gen.h"

#define F_LIST_ITEMS 0x10
#define F_LIST_SIZE 0x18
#define F_ARRAY_LENGTH 0x18
#define F_ARRAY_DATA 0x20
#define F_DROPBOX_REMAIN 0x18
#define F_DROPBOX_MAX 0x1C
#define DROPBOX_VALUE 99

static int g_hits;

static int mod_enabled(void) {
    static int on = -1;
    if (on < 0) {
        const char *v = getenv("DM_MOD_NOLIMITS");
        on = v && v[0] == '1';
        LOG("[mod no-limits] %s (DM_MOD_NOLIMITS=%s)", on ? "on" : "off", v ? v : "unset");
    }
    return on;
}

static void on_equip_drop_box_post(uint64_t *r, int p) {
    (void)p;
    if (!mod_enabled()) return;
    uintptr_t list = (uintptr_t)r[19];
    if (!list) return;
    uintptr_t items = *(const uintptr_t *)(list + F_LIST_ITEMS);
    int size = *(const int *)(list + F_LIST_SIZE);
    if (!items || size <= 0 || size > *(const int *)(items + F_ARRAY_LENGTH)) return;
    for (int i = 0; i < size; i++) {
        uintptr_t entry = *(const uintptr_t *)(items + F_ARRAY_DATA + sizeof(uintptr_t) * (size_t)i);
        if (!entry) continue;
        *(int *)(entry + F_DROPBOX_REMAIN) = DROPBOX_VALUE;
        *(int *)(entry + F_DROPBOX_MAX) = DROPBOX_VALUE;
    }
    if (g_hits++ < 4) LOG("[mod no-limits] equip_drop_box_post: %d entries set to %d", size, DROPBOX_VALUE);
}

__attribute__((constructor)) static void mod_no_limits_attach(void) {
    gate_obs_set(MODGATE_NO_LIMITS_EQUIP_DROP_BOX_POST, on_equip_drop_box_post);
}
