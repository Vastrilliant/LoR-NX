// Based on Bat'ko Makhno's Resolution Fix (Steam Workshop 2893583716)
#include "dm.h"
#include "hle_mods_gen.h"
#include <math.h>

void dm_mod_gate_run(uint64_t *regs, int param, void (*fn)(uint64_t *, int));
void dm_nvn_canvas_size(int *w, int *h);
void dm_nvn_window_size(int *w, int *h);
int dm_opmode(void);

#define K_BUILD_ID "aeea627ddbd1214764dfef5f965e94a2"
#define K_SCREEN_GET_WIDTH 0x17F6530u
#define K_SCREEN_GET_HEIGHT 0x17F6570u
#define K_SCREEN_SET_RESOLUTION 0x17F67D0u
#define K_OBJECT_ALIVE 0x19B3EE0u
#define K_OBJECT_GET_NAME 0x19B4080u
#define K_PTR_TO_STRING_ANSI 0x20EEF30u
#define K_TYPE_GETTYPE 0x26EF270u
#define K_RESOURCES_FINDALL 0x2BFC7E0u
#define K_COMPONENT_GET_TRANSFORM 0x19AE1D0u
#define K_COMPONENT_GET_GAMEOBJECT 0x19AE220u
#define K_COMPONENT_GET_COMPONENT 0x19AE270u
#define K_GO_ACTIVE_IN_HIERARCHY 0x19AFE30u
#define K_TRANSFORM_CHILD_COUNT 0x19BF660u
#define K_TRANSFORM_GET_CHILD 0x19BFE90u
#define K_RT_GET_ANCHOR_MIN 0x19BA280u
#define K_RT_SET_ANCHOR_MIN 0x19BA350u
#define K_RT_GET_ANCHOR_MAX 0x19BA420u
#define K_RT_SET_ANCHOR_MAX 0x19BA4F0u
#define K_RT_GET_OFFSET_MIN 0x19BACD0u
#define K_RT_GET_OFFSET_MAX 0x19BB020u
#define F_ARRAY_LEN 0x18
#define F_ARRAY_DATA 0x20
#define F_SCALER_MODE 0x18
#define F_SCALER_REFERENCE 0x24
#define F_SCALER_MATCH_MODE 0x2C
#define F_SCALER_MATCH 0x30
#define SCALE_WITH_SCREEN_SIZE 1
#define MATCH_WIDTH_OR_HEIGHT 0
#define K_WARMUP_FRAMES 120
#define K_RECHECK_FRAMES 60
#define K_UI_EVERY 30
#define K_MAX_TEXTURE_W 1920
#define K_MAX_TEXTURE_H 1080
#define K_MAX_ATTEMPTS 8
#define K_TYPE_TRIES 8
#define K_LOG_LINES 120
#define K_DESIGN_ASPECT (16.0 / 9.0)

typedef struct { float x, y; } vec2;

static uint8_t *s_main;
static int g_state, g_frame, g_next_check = K_WARMUP_FRAMES, g_attempts, g_applied, g_next_ui, g_type_tries, g_log_lines, g_ui_level = -1;
static void *g_scaler_type, *g_graphic_type;

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

static void ascii_name(void *obj, char *out, size_t cap) {
    void *str = FN(K_OBJECT_GET_NAME, void *, void *, const void *)(obj, NULL);
    size_t n = 0;
    if (str) {
        int len = *(const int *)((uintptr_t)str + 0x10);
        const uint16_t *ch = (const uint16_t *)((uintptr_t)str + 0x14);
        for (int i = 0; i < len && n + 1 < cap; i++) out[n++] = ch[i] < 128 ? (char)ch[i] : '?';
    }
    out[n] = 0;
}

static void *find_type(const char *name) {
    void *str = FN(K_PTR_TO_STRING_ANSI, void *, intptr_t, const void *)((intptr_t)name, NULL);
    if (!str) return NULL;
    return FN(K_TYPE_GETTYPE, void *, void *, const void *)(str, NULL);
}

static int resolve_types(void) {
    if (g_scaler_type && g_graphic_type) return 1;
    if (g_type_tries >= K_TYPE_TRIES) return 0;
    g_type_tries++;
    if (!g_scaler_type) g_scaler_type = find_type("UnityEngine.UI.CanvasScaler, UnityEngine.UI");
    if (!g_graphic_type) g_graphic_type = find_type("UnityEngine.UI.Graphic, UnityEngine.UI");
    LOG("[mod fullscreen-mode] types: CanvasScaler %p, Graphic %p", g_scaler_type, g_graphic_type);
    return g_scaler_type && g_graphic_type;
}

static int round8(double v) { return ((int)(v + 4.0)) & ~7; }

static int target_size(int *w, int *h) {
    int cw = 0, ch = 0;
    dm_nvn_canvas_size(&cw, &ch);
    if (cw <= 0 || ch <= 0) return 0;
    double aspect = (double)cw / ch;
    int base_h = dm_opmode() == 0 ? 720 : 1080;
    int tw = round8(base_h * aspect), th = base_h;
    if (tw > K_MAX_TEXTURE_W) { tw = K_MAX_TEXTURE_W; th = (int)(tw / aspect + 0.5); }
    if (th > K_MAX_TEXTURE_H) th = K_MAX_TEXTURE_H;
    *w = tw; *h = th;
    return tw >= 640 && th >= 360;
}

static int ui_level(void) {
    if (g_ui_level < 0) {
        const char *v = getenv("DM_MOD_FULLSCREEN_UI");
        g_ui_level = v && v[0] >= '0' && v[0] <= '2' ? v[0] - '0' : 2;
        LOG("[mod fullscreen-mode] UI level %d (DM_MOD_FULLSCREEN_UI=%s): 0 resolution only, 1 + canvas scalers match the height, 2 + the layout is centred on a 16:9 area", g_ui_level, v ? v : "unset");
    }
    return g_ui_level;
}

static int near(float a, float b) { return fabsf(a - b) < 0.002f; }

static void layout_children(void *canvas_tf, double aspect, int *moved, int verbose) {
    int n = FN(K_TRANSFORM_CHILD_COUNT, int, void *, const void *)(canvas_tf, NULL);
    double r = K_DESIGN_ASPECT / aspect;
    for (int i = 0; i < n; i++) {
        void *child = FN(K_TRANSFORM_GET_CHILD, void *, void *, int, const void *)(canvas_tf, i, NULL);
        if (!alive(child)) continue;
        vec2 amin = FN(K_RT_GET_ANCHOR_MIN, vec2, void *, const void *)(child, NULL);
        vec2 amax = FN(K_RT_GET_ANCHOR_MAX, vec2, void *, const void *)(child, NULL);
        vec2 omin = FN(K_RT_GET_OFFSET_MIN, vec2, void *, const void *)(child, NULL);
        vec2 omax = FN(K_RT_GET_OFFSET_MAX, vec2, void *, const void *)(child, NULL);
        int stretched = near(amin.x, 0.0f) && near(amax.x, 1.0f) && fabsf(omin.x) < 0.5f && fabsf(omax.x) < 0.5f;
        void *graphic = stretched ? FN(K_COMPONENT_GET_COMPONENT, void *, void *, void *, const void *)(child, g_graphic_type, NULL) : NULL;
        int act = stretched && !alive(graphic) && r < 0.999;
        if (verbose && g_log_lines < K_LOG_LINES) {
            char name[64];
            ascii_name(child, name, sizeof name);
            g_log_lines++;
            LOG("[mod fullscreen-mode]   child \"%s\" anchors (%.3f,%.3f)-(%.3f,%.3f) offsets x %.1f / %.1f, %s -> %s", name, amin.x, amin.y, amax.x, amax.y, omin.x, omax.x,
                !stretched ? "not full width" : alive(graphic) ? "has a graphic" : "plain container", act ? "centred" : "left alone");
        }
        if (!act) continue;
        vec2 nmin = {(float)((1.0 - r) / 2.0), amin.y}, nmax = {(float)((1.0 + r) / 2.0), amax.y};
        FN(K_RT_SET_ANCHOR_MIN, void, void *, vec2, const void *)(child, nmin, NULL);
        FN(K_RT_SET_ANCHOR_MAX, void, void *, vec2, const void *)(child, nmax, NULL);
        (*moved)++;
    }
}

static void sweep_ui(void) {
    g_next_ui = g_frame + K_UI_EVERY;
    int level = ui_level();
    if (level < 1 || !resolve_types()) return;
    int sw = FN(K_SCREEN_GET_WIDTH, int, const void *)(NULL), sh = FN(K_SCREEN_GET_HEIGHT, int, const void *)(NULL);
    if (sw <= 0 || sh <= 0) return;
    double aspect = (double)sw / sh;
    void *arr = FN(K_RESOURCES_FINDALL, void *, void *, const void *)(g_scaler_type, NULL);
    uint64_t n = arr ? *(const uint64_t *)((uintptr_t)arr + F_ARRAY_LEN) : 0;
    int scalers = 0, matched = 0, moved = 0;
    for (uint64_t i = 0; i < n; i++) {
        void *sc = *(void *const *)((uintptr_t)arr + F_ARRAY_DATA + 8 * i);
        if (!alive(sc)) continue;
        void *go = FN(K_COMPONENT_GET_GAMEOBJECT, void *, void *, const void *)(sc, NULL);
        if (!alive(go) || !(FN(K_GO_ACTIVE_IN_HIERARCHY, uint8_t, void *, const void *)(go, NULL) & 1)) continue;
        scalers++;
        uint8_t *p = (uint8_t *)sc;
        int mode = *(int *)(p + F_SCALER_MODE);
        int verbose = 0;
        if (mode == SCALE_WITH_SCREEN_SIZE) {
            float want = aspect >= K_DESIGN_ASPECT ? 1.0f : 0.0f;
            int mm = *(int *)(p + F_SCALER_MATCH_MODE);
            float match = *(float *)(p + F_SCALER_MATCH);
            if (mm != MATCH_WIDTH_OR_HEIGHT || !near(match, want)) {
                if (g_log_lines < K_LOG_LINES) {
                    char name[64];
                    ascii_name(go, name, sizeof name);
                    float *ref = (float *)(p + F_SCALER_REFERENCE);
                    g_log_lines++;
                    LOG("[mod fullscreen-mode] canvas \"%s\": scaler reference %.0fx%.0f, match mode %d, match %.2f -> match %.0f", name, ref[0], ref[1], mm, match, want);
                    verbose = 1;
                }
                *(int *)(p + F_SCALER_MATCH_MODE) = MATCH_WIDTH_OR_HEIGHT;
                *(float *)(p + F_SCALER_MATCH) = want;
                matched++;
            }
        }
        if (level >= 2) layout_children(FN(K_COMPONENT_GET_TRANSFORM, void *, void *, const void *)(sc, NULL), aspect, &moved, verbose);
    }
    static int logged;
    if ((matched || moved) && logged++ < 40) LOG("[mod fullscreen-mode] frame %d: %d of %d canvas scalers set to match the height, %d containers centred (screen %dx%d)", g_frame, matched, scalers, moved, sw, sh);
}

static void frame_run(uint64_t *r, int param) {
    (void)r; (void)param;
    if (g_state < 0 || !main_base()) return;
    if (g_state == 0) {
        if (strncmp(dm_main_build(), K_BUILD_ID, 32) != 0) { g_state = -1; LOG("[mod fullscreen-mode] wrong build, mod stays out"); return; }
        g_state = 1;
    }
    if (++g_frame < g_next_check) return;
    g_next_check = g_frame + K_RECHECK_FRAMES;
    int tw, th;
    if (!target_size(&tw, &th)) {
        static int logged;
        if (logged++ < 3) LOG("[mod fullscreen-mode] frame %d: no usable screen size yet", g_frame);
        return;
    }
    int sw = FN(K_SCREEN_GET_WIDTH, int, const void *)(NULL), sh = FN(K_SCREEN_GET_HEIGHT, int, const void *)(NULL);
    if (sw == tw && sh == th) {
        if (!g_applied) LOG("[mod fullscreen-mode] game renders %dx%d", sw, sh);
        g_applied = 1;
    } else if (g_attempts < K_MAX_ATTEMPTS) {
        g_attempts++;
        FN(K_SCREEN_SET_RESOLUTION, void, int, int, uint8_t, const void *)(tw, th, 1, NULL);
        int pw, ph;
        dm_nvn_window_size(&pw, &ph);
        LOG("[mod fullscreen-mode] frame %d: Screen %dx%d -> SetResolution %dx%d (attempt %d); picture last presented %dx%d", g_frame, sw, sh, tw, th, g_attempts, pw, ph);
    }
}

static void ui_run(uint64_t *r, int param) {
    (void)r; (void)param;
    if (g_state != 1 || !g_applied || g_frame < g_next_ui) return;
    sweep_ui();
}

static void frame_and_ui(uint64_t *r, int param) {
    frame_run(r, param);
    ui_run(r, param);
}

static int mod_enabled(void) {
    static int on = -1;
    if (on < 0) {
        const char *v = getenv("DM_MOD_FULLSCREEN");
        on = v && v[0] == '1';
        LOG("[mod fullscreen-mode] %s (DM_MOD_FULLSCREEN=%s)", on ? "on" : "off", v ? v : "unset");
    }
    return on;
}

static void on_frame_pre(uint64_t *r, int p) { if (mod_enabled()) dm_mod_gate_run(r, p, frame_and_ui); }

__attribute__((constructor)) static void mod_fullscreen_mode_attach(void) {
    gate_obs_set(MODGATE_FULLSCREEN_MODE_FRAME_PRE, on_frame_pre);
}
