#include "dm.h"
#include "dm_input.h"
#include "hle_mods_gen.h"

int dm_touchscreen_mode(void);

#define K_BUILD_ID "aeea627ddbd1214764dfef5f965e94a2"
#define K_RESOLVE_ICALL 0x32F5B4u

#define MASK_SLOTS 128
#define MASK_INTERVAL 2
#define POINTER_REFRESH 6
#define CLIP_INTERVAL 2
#define TEXT_SCALE_INTERVAL 4

static uint8_t *main_base(void) {
    static uint8_t *base;
    if (!base)
        for (int i = 0; i < g_nmod; i++)
            if (!strcmp(g_mod[i].name, "main") && !g_mod[i].nro) { base = g_mod[i].base; break; }
    return base;
}

static int switch_on(const char *name, int *state) {
    if (*state < 0) {
        const char *v = getenv(name);
        *state = v && v[0] == '1';
        LOG("[mod component-list-tail] %s %s", name, *state ? "on" : "off");
    }
    return *state;
}

static int masks_enabled(void) {
    static int master = -1, sub = -1;
    return switch_on("DM_MOD_COMPLISTTAIL", &master) && switch_on("DM_MOD_COMPLISTTAIL_MASKS", &sub);
}

static int pointer_enabled(void) {
    static int master = -1, sub = -1;
    return switch_on("DM_MOD_COMPLISTTAIL", &master) && switch_on("DM_MOD_COMPLISTTAIL_POINTER", &sub);
}

static int clip_enabled(void) {
    static int master = -1, sub = -1;
    return switch_on("DM_MOD_COMPLISTTAIL", &master) && switch_on("DM_MOD_COMPLISTTAIL_CLIP", &sub);
}

static int text_scale_enabled(void) {
    static int master = -1, sub = -1;
    return switch_on("DM_MOD_COMPLISTTAIL", &master) && switch_on("DM_MOD_COMPLISTTAIL_TEXTSCALE", &sub);
}

typedef struct { uint64_t self; uint32_t count; } mask_slot_t;
static mask_slot_t s_mask[MASK_SLOTS];

static int mask_due(uint64_t self) {
    mask_slot_t *s = &s_mask[(self >> 4) & (MASK_SLOTS - 1)];
    if (s->self != self) { s->self = self; s->count = 0; }
    return s->count++ % MASK_INTERVAL == 0;
}

static void on_mask_loop(uint64_t *r, int p) {
    (void)p;
    if (!masks_enabled() || !r[19]) return;
    if (mask_due(r[19])) return;
    r[20] = 0x7fffffffu;
    static int logged;
    if (!logged++) LOG("[mod component-list-tail] sprite mask sync throttled to every %d frames", MASK_INTERVAL);
}

static void on_clip_cull(uint64_t *r, int p) {
    static uint32_t last, tick;
    (void)p;
    if (!clip_enabled()) return;
    uint32_t n = (uint32_t)r[22];
    if (n != last) {
        last = n;
        tick = 0;
        return;
    }
    if (++tick % CLIP_INTERVAL == 0) return;
    r[22] = 0;
    static int logged;
    if (!logged++) LOG("[mod component-list-tail] RectMask2D clipping throttled to every %d frames", CLIP_INTERVAL);
}

static void on_text_scale(uint64_t *r, int p) {
    (void)p;
    if (!text_scale_enabled() || !r[0]) return;
    uint32_t h = (uint32_t)(((r[0] >> 4) * 0x9E3779B97F4A7C15ull) >> 32);
    if ((h + (uint32_t)dm_nvn_frame_count()) % TEXT_SCALE_INTERVAL == 0) return;
    r[8] = 1;
    static int logged;
    if (!logged++) LOG("[mod component-list-tail] text scale check throttled to every %d frames per label", TEXT_SCALE_INTERVAL);
}

static int (*s_frame_count)(void);
static int (*s_real_present)(void *, const void *);

static uint64_t game_frame(void) {
    static int tried;
    if (!tried) {
        tried = 1;
        uint8_t *base = main_base();
        if (base && strncmp(dm_main_build(), K_BUILD_ID, 32) == 0) {
            void *(*resolve)(const char *) = (void *(*)(const char *))(base + K_RESOLVE_ICALL);
            void *fn = resolve("UnityEngine.Time::get_frameCount()");
            if (!fn) fn = resolve("UnityEngine.Time::get_frameCount");
            s_frame_count = (int (*)(void))fn;
        }
    }
    if (s_frame_count) return (uint64_t)(uint32_t)s_frame_count();
    return dm_nvn_frame_count();
}

static int pointer_present(void *self, const void *mi) {
    static int have, skipped;
    static float last_x, last_y;
    int present = s_real_present ? s_real_present(self, mi) : 0;
    if (!present || !dm_touchscreen_mode()) return present;
    dm_mouse_frame_t f;
    dm_mouse_frame(game_frame(), &f);
    int idle = have && !f.held && !f.down && !f.up && !f.wheel && f.nx == last_x && f.ny == last_y;
    last_x = f.nx;
    last_y = f.ny;
    have = 1;
    if (idle && ++skipped < POINTER_REFRESH) return 0;
    skipped = 0;
    return 1;
}

static void on_mouse_present(uint64_t *r, int p) {
    (void)p;
    if (!pointer_enabled() || !dm_touchscreen_mode() || !r[9]) return;
    s_real_present = (int (*)(void *, const void *))(uintptr_t)r[9];
    r[9] = (uint64_t)(uintptr_t)pointer_present;
    static int logged;
    if (!logged++) LOG("[mod component-list-tail] mouse processing skipped while the touchscreen pointer is idle (refresh every %d frames)", POINTER_REFRESH);
}

__attribute__((constructor)) static void mod_component_list_tail_attach(void) {
    gate_obs_set(MODGATE_COMPONENT_LIST_TAIL_MASK_LOOP, on_mask_loop);
    gate_obs_set(MODGATE_COMPONENT_LIST_TAIL_MOUSE_PRESENT, on_mouse_present);
    gate_obs_set(MODGATE_COMPONENT_LIST_TAIL_CLIP_CULL, on_clip_cull);
    gate_obs_set(MODGATE_COMPONENT_LIST_TAIL_TEXT_SCALE, on_text_scale);
}
