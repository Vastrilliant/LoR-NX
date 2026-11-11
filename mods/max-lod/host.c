#include "dm.h"
#include "hle_mods_gen.h"

void dm_mod_gate_run(uint64_t *regs, int param, void (*fn)(uint64_t *, int));

#define K_BUILD_ID "aeea627ddbd1214764dfef5f965e94a2"
#define K_RESOLVE_ICALL 0x32F5B4u
#define K_OBJECT_GETTYPE 0x29B10F0u
#define K_OBJECT_ALIVE 0x19B3EE0u
#define K_TEX2D_WHITE 0x2BE48E0u
#define K_TEX_MIPCOUNT 0x2BE37F0u
#define K_TEX2D_GET_IGNORE 0x2BE4820u
#define K_TEX2D_SET_IGNORE 0x2BE4870u
#define K_RESOURCES_FINDALL 0x2BFC7E0u
#define F_ARRAY_LEN 0x18
#define F_ARRAY_DATA 0x20
#define K_LIMIT_PERIOD 30
#define K_SWEEP_FIRST 120
#define K_SWEEP_BUSY 60
#define K_SWEEP_IDLE 300
#define K_SWEEP_BATCH 48

static uint8_t *s_main;
static int g_state, g_frame, g_next_sweep = K_SWEEP_FIRST;
static int (*g_get_limit)(void);
static void (*g_set_limit)(int);
static int (*g_get_stream)(void);
static void (*g_set_stream)(int);
static const char *g_limit_name = "";

static uint8_t *main_base(void) {
    if (!s_main)
        for (int i = 0; i < g_nmod; i++)
            if (!strcmp(g_mod[i].name, "main") && !g_mod[i].nro) { s_main = g_mod[i].base; break; }
    return s_main;
}

#define FN(rva, ret, ...) ((ret (*)(__VA_ARGS__))(s_main + (rva)))

static void *resolve_icall(const char *cls, const char *prop, const char *suffix) {
    void *(*resolve)(const char *) = (void *(*)(const char *))(s_main + K_RESOLVE_ICALL);
    char name[160];
    static const char *const tails[] = {"", "()"};
    const char *arg = suffix ? suffix : "";
    for (int k = 0; k < 3; k++) {
        if (k == 2) { if (!suffix) break; snprintf(name, sizeof name, "%s::%s%s", cls, prop, arg); }
        else snprintf(name, sizeof name, "%s::%s%s", cls, prop, tails[k]);
        void *fn = resolve(name);
        if (fn) return fn;
    }
    return NULL;
}

static void resolve_all(void) {
    static const char *const limit_props[] = {"globalTextureMipmapLimit", "masterTextureLimit"};
    for (int i = 0; i < 2 && !g_set_limit; i++) {
        char g[64], s[64];
        snprintf(g, sizeof g, "get_%s", limit_props[i]);
        snprintf(s, sizeof s, "set_%s", limit_props[i]);
        g_get_limit = (int (*)(void))resolve_icall("UnityEngine.QualitySettings", g, NULL);
        g_set_limit = (void (*)(int))resolve_icall("UnityEngine.QualitySettings", s, "(System.Int32)");
        if (g_set_limit) g_limit_name = limit_props[i];
    }
    g_get_stream = (int (*)(void))resolve_icall("UnityEngine.QualitySettings", "get_streamingMipmapsActive", NULL);
    g_set_stream = (void (*)(int))resolve_icall("UnityEngine.QualitySettings", "set_streamingMipmapsActive", "(System.Boolean)");
    LOG("[mod max-lod] icalls: %s get %p set %p, streamingMipmapsActive get %p set %p", g_limit_name[0] ? g_limit_name : "mipmap limit not found", (void *)g_get_limit, (void *)g_set_limit, (void *)g_get_stream, (void *)g_set_stream);
}

static void apply_limits(void) {
    static int logged;
    if (g_set_limit) {
        int cur = g_get_limit ? g_get_limit() : -1;
        if (cur != 0) {
            g_set_limit(0);
            if (logged++ < 8) LOG("[mod max-lod] %s %d -> 0 (now %d)", g_limit_name, cur, g_get_limit ? g_get_limit() : -1);
        }
    }
    if (g_set_stream) {
        int cur = g_get_stream ? (g_get_stream() & 1) : 1;
        if (cur) {
            g_set_stream(0);
            if (logged++ < 8) LOG("[mod max-lod] streamingMipmapsActive 1 -> 0 (now %d)", g_get_stream ? g_get_stream() & 1 : -1);
        }
    }
}

static void sweep_textures(void) {
    g_next_sweep = g_frame + K_SWEEP_IDLE;
    void *white = FN(K_TEX2D_WHITE, void *, const void *)(NULL);
    if (!white) return;
    void *type = FN(K_OBJECT_GETTYPE, void *, void *, const void *)(white, NULL);
    if (!type) return;
    void *arr = FN(K_RESOURCES_FINDALL, void *, void *, const void *)(type, NULL);
    if (!arr) return;
    uint64_t n = *(const uint64_t *)((uintptr_t)arr + F_ARRAY_LEN);
    int mipped = 0, changed = 0, pending = 0;
    for (uint64_t i = 0; i < n; i++) {
        void *tex = *(void *const *)((uintptr_t)arr + F_ARRAY_DATA + 8 * i);
        if (!tex || !(FN(K_OBJECT_ALIVE, uint8_t, void *, const void *)(tex, NULL) & 1)) continue;
        if (FN(K_TEX_MIPCOUNT, int, void *, const void *)(tex, NULL) <= 1) continue;
        mipped++;
        if (FN(K_TEX2D_GET_IGNORE, uint8_t, void *, const void *)(tex, NULL) & 1) continue;
        if (changed >= K_SWEEP_BATCH) { pending++; continue; }
        FN(K_TEX2D_SET_IGNORE, void, void *, uint8_t, const void *)(tex, 1, NULL);
        changed++;
    }
    if (pending) g_next_sweep = g_frame + K_SWEEP_BUSY;
    static int logged;
    if (changed || pending || logged < 3) {
        if (logged++ < 60) LOG("[mod max-lod] frame %d: %llu Texture2D, %d with mipmaps, %d set to ignore the mipmap limit, %d waiting", g_frame, (unsigned long long)n, mipped, changed, pending);
    }
}

static void frame_run(uint64_t *r, int param) {
    (void)r; (void)param;
    if (g_state < 0 || !main_base()) return;
    if (g_state == 0) {
        if (strncmp(dm_main_build(), K_BUILD_ID, 32) != 0) { g_state = -1; LOG("[mod max-lod] wrong build, mod stays out"); return; }
        g_state = 1;
        resolve_all();
    }
    g_frame++;
    if (g_frame % K_LIMIT_PERIOD == 5) apply_limits();
    if (g_frame >= g_next_sweep) sweep_textures();
}

static int mod_enabled(void) {
    static int on = -1;
    if (on < 0) {
        const char *v = getenv("DM_MOD_MAXLOD");
        on = v && v[0] == '1';
        LOG("[mod max-lod] %s (DM_MOD_MAXLOD=%s)", on ? "on" : "off", v ? v : "unset");
    }
    return on;
}

static void on_frame_pre(uint64_t *r, int p) { if (mod_enabled()) dm_mod_gate_run(r, p, frame_run); }

__attribute__((constructor)) static void mod_max_lod_attach(void) {
    gate_obs_set(MODGATE_MAX_LOD_FRAME_PRE, on_frame_pre);
}
