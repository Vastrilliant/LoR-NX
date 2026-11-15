#include "dm.h"
#include "hle_mods_gen.h"

void dm_mod_gate_run(uint64_t *regs, int param, void (*fn)(uint64_t *, int));

#define K_BUILD_ID "aeea627ddbd1214764dfef5f965e94a2"
#define K_RESOLVE_ICALL 0x32F5B4u
#define K_OBJECT_GETTYPE 0x29B10F0u
#define K_OBJECT_ALIVE 0x19B3EE0u
#define K_PTR_TO_STRING_ANSI 0x20EEF30u
#define K_TYPE_GETTYPE 0x26EF270u
#define K_TEX2D_WHITE 0x2BE48E0u
#define K_TEX_MIPCOUNT 0x2BE37F0u
#define K_TEX_GET_WIDTH 0x2BE3930u
#define K_TEX_GET_HEIGHT 0x2BE39D0u
#define K_TEX_GET_STREAMING 0x2BE5620u
#define K_TEX_GET_REQUESTED 0x2BE56C0u
#define K_TEX_GET_DESIRED 0x2BE5950u
#define K_TEX_GET_LOADED 0x2BE59F0u
#define K_TEX2D_GET_IGNORE 0x2BE4820u
#define K_TEX2D_SET_IGNORE 0x2BE4870u
#define K_RESOURCES_FINDALL 0x2BFC7E0u
#define K_IMAGE_GET_OVERRIDE_SPRITE 0x24E57E0u
#define K_SPRITE_GET_TEXTURE 0x19C1BD0u
#define K_RAWIMAGE_GET_TEXTURE 0x2969270u
#define F_ARRAY_LEN 0x18
#define F_ARRAY_DATA 0x20
#define K_SWEEP_FIRST 120
#define K_SWEEP_BUSY 60
#define K_SWEEP_IDLE 120
#define K_SWEEP_BATCH 48
#define K_TYPE_TRIES 8
#define K_SEEN_SLOTS 16384
#define K_DIAG_TEXTURES 16

static uint8_t *s_main;
static int g_state, g_frame, g_next_sweep = K_SWEEP_FIRST, g_type_tries;
static void *g_tex_type, *g_image_type, *g_raw_type;

typedef struct {
    int seen, mipped, known, changed, stuck, pending;
} sweep_t;

static uintptr_t g_known[K_SEEN_SLOTS];
static int g_known_count, g_diag;
static int (*g_get_limit)(void);
static const char *g_limit_name = "";

static uint8_t *main_base(void) {
    if (!s_main)
        for (int i = 0; i < g_nmod; i++)
            if (!strcmp(g_mod[i].name, "main") && !g_mod[i].nro) { s_main = g_mod[i].base; break; }
    return s_main;
}

#define FN(rva, ret, ...) ((ret (*)(__VA_ARGS__))(s_main + (rva)))

static int alive(void *obj) {
    return obj && (FN(K_OBJECT_ALIVE, uint8_t, void *, const void *)(obj, NULL) & 1);
}

static int known_has(void *tex) {
    uintptr_t key = (uintptr_t)tex;
    for (uint32_t i = (uint32_t)((key >> 4) * 2654435761u) % K_SEEN_SLOTS, n = 0; n < K_SEEN_SLOTS; i = (i + 1) % K_SEEN_SLOTS, n++) {
        if (g_known[i] == key) return 1;
        if (!g_known[i]) return 0;
    }
    return 0;
}

static void known_add(void *tex) {
    if (g_known_count >= K_SEEN_SLOTS / 2) { memset(g_known, 0, sizeof g_known); g_known_count = 0; }
    uintptr_t key = (uintptr_t)tex;
    uint32_t i = (uint32_t)((key >> 4) * 2654435761u) % K_SEEN_SLOTS;
    while (g_known[i] && g_known[i] != key) i = (i + 1) % K_SEEN_SLOTS;
    if (!g_known[i]) { g_known[i] = key; g_known_count++; }
}

static void *resolve_icall(const char *name) {
    void *(*resolve)(const char *) = (void *(*)(const char *))(s_main + K_RESOLVE_ICALL);
    return resolve(name);
}

static void resolve_limit(void) {
    static const char *const names[] = {"UnityEngine.QualitySettings::get_globalTextureMipmapLimit()", "UnityEngine.QualitySettings::get_globalTextureMipmapLimit", "UnityEngine.QualitySettings::get_masterTextureLimit()", "UnityEngine.QualitySettings::get_masterTextureLimit"};
    for (int i = 0; i < 4 && !g_get_limit; i++) {
        g_get_limit = (int (*)(void))resolve_icall(names[i]);
        if (g_get_limit) g_limit_name = names[i];
    }
    LOG("[mod dynamic-lod] global mipmap limit getter: %s %p", g_limit_name[0] ? g_limit_name : "not found", (void *)g_get_limit);
}

static void *find_type(const char *name) {
    void *str = FN(K_PTR_TO_STRING_ANSI, void *, intptr_t, const void *)((intptr_t)name, NULL);
    if (!str) return NULL;
    return FN(K_TYPE_GETTYPE, void *, void *, const void *)(str, NULL);
}

static int resolve_types(void) {
    int ready = g_tex_type && (g_image_type || g_raw_type);
    if (ready || g_type_tries >= K_TYPE_TRIES) return ready;
    g_type_tries++;
    if (!g_tex_type) {
        void *white = FN(K_TEX2D_WHITE, void *, const void *)(NULL);
        if (white) g_tex_type = FN(K_OBJECT_GETTYPE, void *, void *, const void *)(white, NULL);
    }
    if (!g_image_type) g_image_type = find_type("UnityEngine.UI.Image, UnityEngine.UI");
    if (!g_raw_type) g_raw_type = find_type("UnityEngine.UI.RawImage, UnityEngine.UI");
    LOG("[mod dynamic-lod] types: Texture2D %p, Image %p, RawImage %p", g_tex_type, g_image_type, g_raw_type);
    if (!g_get_limit && !g_limit_name[0]) resolve_limit();
    return g_tex_type && (g_image_type || g_raw_type);
}

static void *objects_of(void *type, uint64_t *n) {
    *n = 0;
    if (!type) return NULL;
    void *arr = FN(K_RESOURCES_FINDALL, void *, void *, const void *)(type, NULL);
    if (arr) *n = *(const uint64_t *)((uintptr_t)arr + F_ARRAY_LEN);
    return arr;
}

static void diag(void *tex, int before, int after) {
    if (g_diag++ >= K_DIAG_TEXTURES) return;
    LOG("[mod dynamic-lod] texture %p %dx%d, %d mips, streaming %d, requested %d, desired %d, loaded %d, ignore %d -> %d",
        tex,
        FN(K_TEX_GET_WIDTH, int, void *, const void *)(tex, NULL),
        FN(K_TEX_GET_HEIGHT, int, void *, const void *)(tex, NULL),
        FN(K_TEX_MIPCOUNT, int, void *, const void *)(tex, NULL),
        FN(K_TEX_GET_STREAMING, uint8_t, void *, const void *)(tex, NULL) & 1,
        FN(K_TEX_GET_REQUESTED, int, void *, const void *)(tex, NULL),
        FN(K_TEX_GET_DESIRED, int, void *, const void *)(tex, NULL),
        FN(K_TEX_GET_LOADED, int, void *, const void *)(tex, NULL),
        before, after);
}

static void upgrade(void *tex, sweep_t *s) {
    if (!alive(tex)) return;
    s->seen++;
    if (FN(K_TEX_MIPCOUNT, int, void *, const void *)(tex, NULL) <= 1) return;
    s->mipped++;
    if (known_has(tex)) { s->known++; return; }
    int before = FN(K_TEX2D_GET_IGNORE, uint8_t, void *, const void *)(tex, NULL) & 1;
    if (before) { known_add(tex); s->known++; return; }
    if (s->changed >= K_SWEEP_BATCH) { s->pending++; return; }
    FN(K_TEX2D_SET_IGNORE, void, void *, uint8_t, const void *)(tex, 1, NULL);
    int after = FN(K_TEX2D_GET_IGNORE, uint8_t, void *, const void *)(tex, NULL) & 1;
    diag(tex, before, after);
    known_add(tex);
    s->changed++;
    if (!after) s->stuck++;
}

static void sweep_images(sweep_t *s) {
    uint64_t n;
    void *arr = objects_of(g_image_type, &n);
    for (uint64_t i = 0; arr && i < n; i++) {
        void *img = *(void *const *)((uintptr_t)arr + F_ARRAY_DATA + 8 * i);
        if (!alive(img)) continue;
        void *sprite = FN(K_IMAGE_GET_OVERRIDE_SPRITE, void *, void *, const void *)(img, NULL);
        if (!alive(sprite)) continue;
        upgrade(FN(K_SPRITE_GET_TEXTURE, void *, void *, const void *)(sprite, NULL), s);
    }
}

static void sweep_raw_images(sweep_t *s) {
    uint64_t n;
    void *arr = objects_of(g_raw_type, &n);
    for (uint64_t i = 0; arr && i < n; i++) {
        void *raw = *(void *const *)((uintptr_t)arr + F_ARRAY_DATA + 8 * i);
        if (!alive(raw)) continue;
        void *tex = FN(K_RAWIMAGE_GET_TEXTURE, void *, void *, const void *)(raw, NULL);
        if (!alive(tex)) continue;
        if (FN(K_OBJECT_GETTYPE, void *, void *, const void *)(tex, NULL) != g_tex_type) continue;
        upgrade(tex, s);
    }
}

static void sweep_textures(void) {
    g_next_sweep = g_frame + K_SWEEP_IDLE;
    if (!resolve_types()) return;
    sweep_t s = {0};
    sweep_images(&s);
    sweep_raw_images(&s);
    if (s.pending) g_next_sweep = g_frame + K_SWEEP_BUSY;
    static int logged;
    if (s.changed || s.pending || logged < 3) {
        if (logged++ < 60) LOG("[mod dynamic-lod] frame %d: %d UI textures, %d with mipmaps, %d already done, %d set to ignore the mipmap limit (%d did not stick), %d waiting, global mipmap limit %d", g_frame, s.seen, s.mipped, s.known, s.changed, s.stuck, s.pending, g_get_limit ? g_get_limit() : -1);
    }
}

static void frame_run(uint64_t *r, int param) {
    (void)r; (void)param;
    if (g_state < 0 || !main_base()) return;
    if (g_state == 0) {
        if (strncmp(dm_main_build(), K_BUILD_ID, 32) != 0) { g_state = -1; LOG("[mod dynamic-lod] wrong build, mod stays out"); return; }
        g_state = 1;
    }
    g_frame++;
    if (g_frame >= g_next_sweep) sweep_textures();
}

static int mod_enabled(void) {
    static int on = -1;
    if (on < 0) {
        const char *v = getenv("DM_MOD_DYNAMICLOD");
        on = v && v[0] == '1';
        LOG("[mod dynamic-lod] %s (DM_MOD_DYNAMICLOD=%s)", on ? "on" : "off", v ? v : "unset");
    }
    return on;
}

static void on_frame_pre(uint64_t *r, int p) { if (mod_enabled()) dm_mod_gate_run(r, p, frame_run); }

__attribute__((constructor)) static void mod_dynamic_lod_attach(void) {
    gate_obs_set(MODGATE_DYNAMIC_LOD_FRAME_PRE, on_frame_pre);
}
