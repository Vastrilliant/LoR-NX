// Based on Bat'ko Makhno's Resolution Fix (Steam Workshop 2893583716)
#include "dm.h"
#include "front.h"
#include "dm_input.h"
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
#define K_RT_SET_OFFSET_MIN 0x19BADB0u
#define K_RT_SET_OFFSET_MAX 0x19BB150u
#define K_RT_GET_RECT 0x19BA1B0u
#define K_OBJECT_INSTANTIATE_PARENT 0x19B4E90u
#define K_IMAGE_SET_SPRITE 0x24D56D0u
#define K_GRAPHIC_SET_COLOR 0x24DE110u
#define K_GRAPHIC_SET_RAYCAST 0x24DE170u
#define K_TRANSFORM_SET_LOCALSCALE 0x19BD790u
#define K_TRANSFORM_SET_AS_LAST_SIBLING 0x19BF750u
#define K_OBJECT_DESTROY 0x19B5310u
#define K_TIME_FRAMECOUNT 0x19B9380u
#define K_RT_GET_PIVOT 0x19BA900u
#define K_TRANSFORM_GET_LOSSY_SCALE 0x19BFA20u
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
#define K_UI_EVERY 20
#define K_MAX_TEXTURE_W 1920
#define K_MAX_TEXTURE_H 1080
#define K_MAX_ATTEMPTS 8
#define K_TYPE_TRIES 8
#define K_LOG_LINES 300
#define K_LOG_LINES_VERBOSE 4000
#define K_MAX_DEPTH 10
#define K_NODE_BUDGET 8000
#define K_REC_CAP 16384
#define K_FULL_EVERY 10
#define K_UNION_MAX 64
#define K_NOTE_CAP 128
#define K_FULL_TOL 2.0
#define K_WIDE_FRAC 0.7
#define K_BAND_FRAC 0.5
#define K_CENTER_FRAC 0.02
#define K_MIDDLE_FRAC 0.10
#define K_EDGE_FRAC 0.06
#define K_STRETCH_MARGIN 0.08
#define K_POINT_FRAC 0.05
#define K_GROUP_EDGE 0.06
#define K_GROUP_OFF 0.20
#define K_GROUP_GAP 0.08
#define K_ART_DEPTH 1
#define K_SCALE_MIN 0.05
#define K_APPLY_EPS 0.5
#define K_DESIGN_ASPECT (16.0 / 9.0)
#define K_OV_MAX 512
#define K_TAB 4096
#define K_KEY_MAX 320
#define K_SEG_MAX 48
#define K_PICK_MAX 48
#define K_PICK_DEPTH 24
#define K_SAME_SPOT 0.012f
#define K_HL_ALPHA 0.4f
#define K_LAYOUT_FILE "fullscreen_layout.txt"

typedef struct { float x, y; } vec2;
typedef struct { float x, y, z; } vec3;
typedef struct { float x, y, w, h; } rect4;
typedef struct { void *obj; float oa[4], oo[4], aa[4], ao[4], sh; uint32_t kh; uint8_t seen, logged, vlog, keep, kind, act; } rec_t;
typedef struct { int ax; double size, design, osize, cscale; } cv_t;
typedef struct { double des0, dsz, act0, asz, kp, v0, vsz; int grp, through; } par_t;
typedef struct { float r, g, b, a; } rgba;
typedef struct { double x0, xs, y0, ys, kp; } pk_t;
typedef struct { uint32_t h; uint8_t mode; float dx, dy; char key[K_KEY_MAX]; } ov_t;
typedef struct { uint32_t h; int16_t ov; uint8_t anc; } slot_t;
typedef struct { void *tf; uint32_t h; int depth; double area; char key[K_KEY_MAX]; } pick_t;

enum { OV_AUTO, OV_LEFT, OV_CENTER, OV_RIGHT, OV_STRETCH, OV_EXPAND, OV_COUNT };

static const char *const k_ov_name[OV_COUNT] = {"auto", "left", "center", "right", "stretch", "expand"};
static const rgba k_ov_color[OV_COUNT] = {{1.0f, 1.0f, 1.0f, K_HL_ALPHA}, {0.2f, 0.5f, 1.0f, K_HL_ALPHA}, {0.2f, 1.0f, 0.4f, K_HL_ALPHA}, {1.0f, 0.6f, 0.1f, K_HL_ALPHA}, {1.0f, 0.2f, 0.9f, K_HL_ALPHA}, {1.0f, 0.15f, 0.15f, K_HL_ALPHA}};

enum { ACT_SKIP, ACT_LEFT, ACT_RIGHT, ACT_CENTER, ACT_STRETCH, ACT_COVER, ACT_THRU, ACT_SCALED, ACT_COUNT };

static const char *const k_act_name[ACT_COUNT] = {"outside the design area, left alone", "left", "right", "centre", "stretch", "expanded to cover", "container, children placed one by one", "scaled, left alone"};

static uint8_t *s_main;
static int g_state, g_frame, g_next_check = K_WARMUP_FRAMES, g_attempts, g_applied, g_next_ui, g_type_tries, g_log_lines, g_ui_level = -1;
static int g_sweep, g_start, g_starved, g_skip_inactive, s_noted_n;
static void *s_noted[K_NOTE_CAP];
static void *g_scaler_type, *g_layout_type, *g_scroll_type, *s_rt_klass;
static rec_t s_rec[K_REC_CAP];
static int s_rec_n, g_budget, g_changed, g_act[ACT_COUNT];
static ov_t s_ov[K_OV_MAX];
static slot_t s_tab[K_TAB];
static int s_ov_n, s_paths, s_idx, s_edit = -1, s_select, s_loaded, s_pick_n, s_pick_i, s_hold[4];
static uint32_t s_par_h[K_MAX_DEPTH + 4], s_sel_h;
static char s_pseg[K_PICK_DEPTH + 2][K_SEG_MAX], s_sel_key[K_KEY_MAX];
static pick_t s_pick[K_PICK_MAX];
static void *s_sel_tf, *s_hl_img, *s_hl_tf, *s_hl_src, *g_image_type;
static float s_last_nx = -1.0f, s_last_ny = -1.0f;
static uint64_t s_prev_keys;

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
    if (g_scaler_type && g_layout_type && g_scroll_type) return 1;
    if (g_type_tries < K_TYPE_TRIES) {
        g_type_tries++;
        if (!g_scaler_type) g_scaler_type = find_type("UnityEngine.UI.CanvasScaler, UnityEngine.UI");
        if (!g_layout_type) g_layout_type = find_type("UnityEngine.UI.LayoutGroup, UnityEngine.UI");
        if (!g_scroll_type) g_scroll_type = find_type("UnityEngine.UI.ScrollRect, UnityEngine.UI");
        if (!g_image_type) g_image_type = find_type("UnityEngine.UI.Image, UnityEngine.UI");
        LOG("[mod fullscreen-mode] types: CanvasScaler %p, LayoutGroup %p, ScrollRect %p", g_scaler_type, g_layout_type, g_scroll_type);
    }
    return g_scaler_type != NULL;
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
        g_ui_level = v && v[0] >= '0' && v[0] <= '3' ? v[0] - '0' : 2;
        LOG("[mod fullscreen-mode] UI level %d (DM_MOD_FULLSCREEN_UI=%s): 0 resolution only, 1 + canvas scalers match the height or width, 2 + every element is moved to the screen's corner or edge, centred, stretched or expanded, 3 + every element's decision is logged", g_ui_level, v ? v : "unset");
    }
    return g_ui_level;
}

static int near(float a, float b) { return fabsf(a - b) < 0.002f; }

static int log_ok(void) { return g_log_lines < (g_ui_level >= 3 ? K_LOG_LINES_VERBOSE : K_LOG_LINES); }

static int first_note(void *obj) {
    for (int i = 0; i < s_noted_n; i++)
        if (s_noted[i] == obj) return 0;
    if (s_noted_n < K_NOTE_CAP) s_noted[s_noted_n++] = obj;
    return 1;
}

static int same4(const float *a, const float *b, float tol) {
    for (int i = 0; i < 4; i++)
        if (fabsf(a[i] - b[i]) > tol) return 0;
    return 1;
}

static void rt_read(void *tf, float *a, float *o) {
    vec2 amin = FN(K_RT_GET_ANCHOR_MIN, vec2, void *, const void *)(tf, NULL);
    vec2 amax = FN(K_RT_GET_ANCHOR_MAX, vec2, void *, const void *)(tf, NULL);
    vec2 omin = FN(K_RT_GET_OFFSET_MIN, vec2, void *, const void *)(tf, NULL);
    vec2 omax = FN(K_RT_GET_OFFSET_MAX, vec2, void *, const void *)(tf, NULL);
    a[0] = amin.x; a[1] = amin.y; a[2] = amax.x; a[3] = amax.y;
    o[0] = omin.x; o[1] = omin.y; o[2] = omax.x; o[3] = omax.y;
}

static void rt_write_anchors(void *tf, const float *a) {
    vec2 amin = {a[0], a[1]}, amax = {a[2], a[3]};
    FN(K_RT_SET_ANCHOR_MIN, void, void *, vec2, const void *)(tf, amin, NULL);
    FN(K_RT_SET_ANCHOR_MAX, void, void *, vec2, const void *)(tf, amax, NULL);
}

static void rt_write_offsets(void *tf, const float *o) {
    vec2 omin = {o[0], o[1]}, omax = {o[2], o[3]};
    FN(K_RT_SET_OFFSET_MIN, void, void *, vec2, const void *)(tf, omin, NULL);
    FN(K_RT_SET_OFFSET_MAX, void, void *, vec2, const void *)(tf, omax, NULL);
}

static double lossy_scale(void *tf, int ax) {
    vec3 s = FN(K_TRANSFORM_GET_LOSSY_SCALE, vec3, void *, const void *)(tf, NULL);
    return fabs(ax ? s.y : s.x);
}

static rec_t *rec_find(void *obj) {
    uint32_t h = (uint32_t)(((uintptr_t)obj >> 4) * 2654435761u);
    for (uint32_t i = 0; i < K_REC_CAP; i++) {
        rec_t *r = &s_rec[(h + i) % K_REC_CAP];
        if (r->obj == obj) return r;
        if (!r->obj) return NULL;
    }
    return NULL;
}

static rec_t *rec_get(void *obj) {
    uint32_t h = (uint32_t)(((uintptr_t)obj >> 4) * 2654435761u);
    for (uint32_t i = 0; i < K_REC_CAP; i++) {
        rec_t *r = &s_rec[(h + i) % K_REC_CAP];
        if (r->obj == obj) return r;
        if (!r->obj) {
            if (s_rec_n >= K_REC_CAP - K_REC_CAP / 8) return NULL;
            r->obj = obj;
            s_rec_n++;
            return r;
        }
    }
    return NULL;
}

static void rec_compact(void) {
    if (s_rec_n < K_REC_CAP / 2) return;
    int n = 0;
    for (int i = 0; i < K_REC_CAP; i++)
        if (s_rec[i].obj && s_rec[i].keep) n++;
    rec_t *keep = malloc((size_t)(n ? n : 1) * sizeof *keep);
    if (!keep) return;
    int k = 0, before = s_rec_n;
    for (int i = 0; i < K_REC_CAP; i++)
        if (s_rec[i].obj && s_rec[i].keep) keep[k++] = s_rec[i];
    memset(s_rec, 0, sizeof s_rec);
    s_rec_n = 0;
    for (int i = 0; i < k; i++) {
        rec_t *r = rec_get(keep[i].obj);
        if (r) *r = keep[i];
    }
    free(keep);
    LOG("[mod fullscreen-mode] element table compacted: %d entries, %d kept (changed or container)", before, k);
}

static void rec_sync(rec_t *r, const float *ca, const float *co) {
    if (!r->seen) {
        r->seen = 1;
        memcpy(r->oa, ca, sizeof r->oa); memcpy(r->oo, co, sizeof r->oo);
        memcpy(r->aa, ca, sizeof r->aa); memcpy(r->ao, co, sizeof r->ao);
        return;
    }
    if (!same4(ca, r->aa, 1e-5f)) memcpy(r->oa, ca, sizeof r->oa);
    if (!same4(co, r->ao, 1e-3f)) memcpy(r->oo, co, sizeof r->oo);
}

static int classify(double d0, double d1, const cv_t *cv, int art, double *t0, double *t1) {
    double dw = cv->design, wc = cv->size, pad = (wc - dw) / 2.0, mid = dw / 2.0;
    double w = d1 - d0, ml = d0, mr = dw - d1, c = (d0 + d1) / 2.0, off = fabs(c - mid);
    int outside = w > 1.0 ? (d1 <= K_APPLY_EPS || d0 >= dw - K_APPLY_EPS) : (d1 < -K_APPLY_EPS || d0 > dw + K_APPLY_EPS);
    if (outside) return ACT_SKIP;
    if (ml <= K_FULL_TOL && mr <= K_FULL_TOL) {
        if (art) {
            double f = wc / dw;
            *t0 = wc / 2.0 + (d0 - mid) * f;
            *t1 = wc / 2.0 + (d1 - mid) * f;
            return ACT_COVER;
        }
        *t0 = 0.0;
        *t1 = wc;
        return ACT_STRETCH;
    }
    if ((ml <= K_STRETCH_MARGIN * dw && mr <= K_STRETCH_MARGIN * dw) || (off <= K_CENTER_FRAC * dw && w >= K_BAND_FRAC * dw)) {
        *t0 = ml;
        *t1 = wc - mr;
        if (*t1 < *t0) *t1 = *t0;
        return ACT_STRETCH;
    }
    double sh;
    int act;
    if (w >= K_BAND_FRAC * dw && ml <= K_EDGE_FRAC * dw && ml < mr) { sh = 0.0; act = ACT_LEFT; }
    else if (w >= K_BAND_FRAC * dw && mr <= K_EDGE_FRAC * dw && mr < ml) { sh = 2.0 * pad; act = ACT_RIGHT; }
    else if (w >= K_BAND_FRAC * dw || off <= K_MIDDLE_FRAC * dw) { sh = pad; act = ACT_CENTER; }
    else if (c < mid) { sh = 0.0; act = ACT_LEFT; }
    else { sh = 2.0 * pad; act = ACT_RIGHT; }
    *t0 = d0 + sh;
    *t1 = d1 + sh;
    return act;
}

static int opaque(void *tf) {
    if (g_layout_type && alive(FN(K_COMPONENT_GET_COMPONENT, void *, void *, void *, const void *)(tf, g_layout_type, NULL))) return 1;
    if (g_scroll_type && alive(FN(K_COMPONENT_GET_COMPONENT, void *, void *, void *, const void *)(tf, g_scroll_type, NULL))) return 1;
    return 0;
}

static int active_node(void *tf) {
    void *go = FN(K_COMPONENT_GET_GAMEOBJECT, void *, void *, const void *)(tf, NULL);
    return alive(go) && (FN(K_GO_ACTIVE_IN_HIERARCHY, uint8_t, void *, const void *)(go, NULL) & 1);
}

static int child_count(void *tf) {
    return FN(K_TRANSFORM_CHILD_COUNT, int, void *, const void *)(tf, NULL);
}

static void *child_at(void *tf, int i) {
    void *child = FN(K_TRANSFORM_GET_CHILD, void *, void *, int, const void *)(tf, i, NULL);
    return alive(child) && *(void *const *)child == s_rt_klass ? child : NULL;
}

static uint32_t fnv(uint32_t h, const char *str) {
    while (*str) { h ^= (uint8_t)*str++; h *= 16777619u; }
    return h ? h : 1u;
}

static slot_t *slot_get(uint32_t h, int make) {
    for (uint32_t i = 0; i < K_TAB; i++) {
        slot_t *sl = &s_tab[(h + i) & (K_TAB - 1)];
        if (sl->h == h) return sl;
        if (!sl->h) {
            if (!make) return NULL;
            sl->h = h;
            sl->ov = -1;
            return sl;
        }
    }
    return NULL;
}

static void seg_clean(char *str) {
    for (; *str; str++)
        if (*str == '/' || *str == '|' || *str == ' ' || *str == '[' || *str == ']' || *str == '\t' || *str == '\n' || *str == '\r') *str = '_';
}

static void seg_make(void *tf, int idx, char *out) {
    char name[K_SEG_MAX - 8];
    ascii_name(tf, name, sizeof name);
    seg_clean(name);
    snprintf(out, K_SEG_MAX, "%s[%d]", name, idx);
}

static void canvas_root(void *go, char *out) {
    char name[K_SEG_MAX - 4];
    ascii_name(go, name, sizeof name);
    seg_clean(name);
    snprintf(out, K_SEG_MAX, "c:%s", name);
}

static uint32_t key_hash(const char *key, int register_ancestors) {
    char buf[K_KEY_MAX];
    snprintf(buf, sizeof buf, "%s", key);
    char *save = NULL, *tok = strtok_r(buf, "/", &save);
    uint32_t h = 0;
    int first = 1;
    while (tok) {
        h = first ? fnv(2166136261u, tok) : fnv(fnv(h, "/"), tok);
        first = 0;
        tok = strtok_r(NULL, "/", &save);
        if (tok && register_ancestors) {
            slot_t *sl = slot_get(h, 1);
            if (sl) sl->anc = 1;
        }
    }
    return h;
}

static int mode_parse(const char *name) {
    for (int i = 0; i < OV_COUNT; i++)
        if (!strncmp(name, k_ov_name[i], strlen(k_ov_name[i]))) return i;
    return -1;
}

static ov_t *ov_find(uint32_t h) {
    slot_t *sl = slot_get(h, 0);
    return sl && sl->ov >= 0 ? &s_ov[sl->ov] : NULL;
}

static ov_t *ov_make(const char *key) {
    uint32_t h = key_hash(key, 1);
    if (!h) return NULL;
    ov_t *o = ov_find(h);
    if (o) return o;
    slot_t *sl = slot_get(h, 1);
    if (!sl) return NULL;
    int idx = -1;
    for (int i = 0; i < s_ov_n; i++)
        if (!s_ov[i].key[0]) { idx = i; break; }
    if (idx < 0) {
        if (s_ov_n >= K_OV_MAX) return NULL;
        idx = s_ov_n++;
    }
    o = &s_ov[idx];
    memset(o, 0, sizeof *o);
    o->h = h;
    snprintf(o->key, sizeof o->key, "%s", key);
    sl->ov = (int16_t)idx;
    s_paths = 1;
    return o;
}

static void ov_drop(ov_t *o) {
    slot_t *sl = slot_get(o->h, 0);
    if (sl) sl->ov = -1;
    memset(o, 0, sizeof *o);
}

static const char *layout_path(void) {
    const char *v = getenv("DM_MOD_FULLSCREEN_LAYOUT");
    return v && v[0] ? v : K_LAYOUT_FILE;
}

static void layout_load(void) {
    FILE *f = fopen(layout_path(), "r");
    if (!f) { LOG("[mod fullscreen-mode] layout: no file %s, nothing to load", layout_path()); return; }
    char line[K_KEY_MAX + 64];
    int n = 0;
    while (fgets(line, sizeof line, f)) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
        char *save = NULL, *key = strtok_r(line, "|", &save), *mode = strtok_r(NULL, "|", &save), *dx = strtok_r(NULL, "|", &save), *dy = strtok_r(NULL, "|\r\n", &save);
        int m = mode ? mode_parse(mode) : -1;
        if (!key || m < 0) continue;
        ov_t *o = ov_make(key);
        if (!o) continue;
        o->mode = (uint8_t)m;
        o->dx = dx ? strtof(dx, NULL) : 0.0f;
        o->dy = dy ? strtof(dy, NULL) : 0.0f;
        n++;
    }
    fclose(f);
    LOG("[mod fullscreen-mode] layout: %d placements loaded from %s", n, layout_path());
}

static void layout_save(void) {
    FILE *f = fopen(layout_path(), "w");
    if (!f) { LOG("[mod fullscreen-mode] layout: cannot write %s", layout_path()); return; }
    fprintf(f, "# fullscreen-mode layout, one placement per line: canvas path|mode|dx|dy (design units, dy up)\n");
    int n = 0;
    for (int i = 0; i < s_ov_n; i++) {
        const ov_t *o = &s_ov[i];
        if (!o->key[0]) continue;
        fprintf(f, "%s|%s|%.1f|%.1f\n", o->key, k_ov_name[o->mode], o->dx, o->dy);
        n++;
    }
    fclose(f);
    LOG("[mod fullscreen-mode] layout: %d placements saved to %s", n, layout_path());
}

static int ov_apply(int mode, double d0, double d1, const cv_t *cv, double *t0, double *t1) {
    double wc = cv->size, dw = cv->design, pad = (wc - dw) / 2.0, mid = dw / 2.0;
    switch (mode) {
    case OV_LEFT: *t0 = d0; *t1 = d1; return ACT_LEFT;
    case OV_CENTER: *t0 = d0 + pad; *t1 = d1 + pad; return ACT_CENTER;
    case OV_RIGHT: *t0 = d0 + 2.0 * pad; *t1 = d1 + 2.0 * pad; return ACT_RIGHT;
    case OV_STRETCH:
        *t0 = d0;
        *t1 = wc - (dw - d1);
        if (*t1 < *t0) *t1 = *t0;
        return ACT_STRETCH;
    case OV_EXPAND: {
        double f = wc / dw;
        *t0 = wc / 2.0 + (d0 - mid) * f;
        *t1 = wc / 2.0 + (d1 - mid) * f;
        return ACT_COVER;
    }
    }
    return ACT_SKIP;
}

static void child_par(par_t *np, const cv_t *cv, const par_t *pp, const rec_t *r, const float *ca, const float *co, double d0, double lo, double hi, double a0, double v0, double kpn) {
    int ax = cv->ax, ay = 1 - ax;
    np->des0 = d0;
    np->dsz = hi - lo;
    np->act0 = a0;
    np->asz = ((double)ca[2 + ax] - ca[ax]) * pp->asz + ((double)co[2 + ax] - co[ax]);
    np->kp = kpn;
    np->v0 = v0;
    np->vsz = ((double)r->oa[2 + ay] - r->oa[ay]) * pp->vsz + ((double)r->oo[2 + ay] - r->oo[ay]);
    np->grp = 0;
    np->through = 0;
}

static int place_axis(void *tf, int ax, double org, double psz, double kp, double t0, double t1, double *o0, double *o1) {
    float ca[4], co[4];
    rt_read(tf, ca, co);
    double a0 = org + (ca[ax] * psz + co[ax]) * kp;
    double a1 = org + (ca[2 + ax] * psz + co[2 + ax]) * kp;
    double e0 = t0 - a0, e1 = t1 - a1;
    int changed = 0;
    if (fabs(e0) > K_APPLY_EPS || fabs(e1) > K_APPLY_EPS) {
        double den = psz * kp;
        if (den > 1e-3) {
            float na[4];
            memcpy(na, ca, sizeof na);
            na[ax] += (float)(e0 / den);
            na[2 + ax] += (float)(e1 / den);
            rt_write_anchors(tf, na);
            rt_read(tf, ca, co);
            a0 = org + (ca[ax] * psz + co[ax]) * kp;
            a1 = org + (ca[2 + ax] * psz + co[2 + ax]) * kp;
            e0 = t0 - a0;
            e1 = t1 - a1;
        }
        if (fabs(e0) > K_APPLY_EPS || fabs(e1) > K_APPLY_EPS) {
            float no[4];
            memcpy(no, co, sizeof no);
            no[ax] += (float)(e0 / kp);
            no[2 + ax] += (float)(e1 / kp);
            rt_write_offsets(tf, no);
            rt_read(tf, ca, co);
            a0 = org + (ca[ax] * psz + co[ax]) * kp;
            a1 = org + (ca[2 + ax] * psz + co[2 + ax]) * kp;
        }
        changed = 1;
    }
    *o0 = a0;
    *o1 = a1;
    return changed;
}

static int children_union(void *tf, const cv_t *cv, const par_t *np, double *u0, double *u1) {
    int n = child_count(tf), ax = cv->ax, got = 0;
    if (n <= 0 || n > K_UNION_MAX) return 0;
    double lo = 0.0, hi = 0.0;
    for (int i = 0; i < n; i++) {
        void *child = child_at(tf, i);
        if (!child) continue;
        g_budget--;
        float ca[4], co[4];
        rt_read(child, ca, co);
        rec_t *cr = rec_find(child);
        int keep_a = cr && cr->seen && same4(ca, cr->aa, 1e-5f), keep_o = cr && cr->seen && same4(co, cr->ao, 1e-3f);
        const float *oa = keep_a ? cr->oa : ca, *oo = keep_o ? cr->oo : co;
        double c0 = oa[ax] * np->dsz + oo[ax], c1 = oa[2 + ax] * np->dsz + oo[2 + ax];
        double a = np->des0 + fmin(c0, c1) * np->kp, b = np->des0 + fmax(c0, c1) * np->kp;
        if (b < -K_APPLY_EPS || a > cv->design + K_APPLY_EPS) continue;
        if (!got || a < lo) lo = a;
        if (!got || b > hi) hi = b;
        got = 1;
    }
    *u0 = lo;
    *u1 = hi;
    return got;
}

static int settle_container(void *tf, rec_t *r, const cv_t *cv, double d0, double d1, double dsz, double kp, int nchild, int act, double *t0, double *t1) {
    if (r->kind == 2) return ACT_THRU;
    if (r->kind == 1) {
        *t0 = d0 + r->sh;
        *t1 = d1 + r->sh;
        return r->act;
    }
    if (nchild <= 0 || d1 - d0 > K_POINT_FRAC * cv->design || opaque(tf)) return act;
    par_t dp = {d0, dsz, 0.0, 0.0, kp, 0.0, 0.0, 0, 0};
    double u0, u1, sh = *t0 - d0;
    if (children_union(tf, cv, &dp, &u0, &u1)) {
        double e0 = fmin(d0, u0), e1 = fmax(d1, u1), x0 = 0.0, x1 = 0.0;
        int a2 = classify(e0, e1, cv, 0, &x0, &x1);
        if (a2 == ACT_STRETCH) {
            r->kind = 2;
            r->keep = 1;
            return ACT_THRU;
        }
        if (a2 != ACT_SKIP) {
            act = a2;
            sh = x0 - e0;
        }
    }
    r->kind = 1;
    r->keep = 1;
    r->act = (uint8_t)act;
    r->sh = (float)sh;
    *t0 = d0 + sh;
    *t1 = d1 + sh;
    return act;
}

static void process_children(void *tf, const cv_t *cv, const par_t *pp, int depth);

static void process_node(void *tf, const cv_t *cv, const par_t *pp, int depth, int *bleed) {
    if (g_budget <= 0) { g_starved = 1; return; }
    g_budget--;
    int ax = cv->ax, ay = 1 - ax;
    double s = lossy_scale(tf, ax);
    double srel = cv->cscale > 1e-6 && pp->kp > 1e-6 ? s / cv->cscale / pp->kp : 1.0;
    double kpn = cv->cscale > 1e-6 && s > 1e-6 ? s / cv->cscale : 1.0;
    float ca[4], co[4];
    rt_read(tf, ca, co);
    rec_t *r = rec_get(tf);
    if (!r) {
        static int full_logged;
        if (!full_logged++) LOG("[mod fullscreen-mode] element table full (%d), new elements are left alone", K_REC_CAP);
        return;
    }
    rec_sync(r, ca, co);
    ov_t *ov = NULL;
    int anc = 0;
    if (s_paths) {
        if (!r->kh) {
            char seg[K_SEG_MAX];
            seg_make(tf, s_idx, seg);
            r->kh = fnv(fnv(s_par_h[depth], "/"), seg);
        }
        s_par_h[depth + 1] = r->kh;
        slot_t *sl = slot_get(r->kh, 0);
        if (sl) { ov = sl->ov >= 0 ? &s_ov[sl->ov] : NULL; anc = sl->anc; }
    }
    double c0 = r->oa[ax] * pp->dsz + r->oo[ax];
    double c1 = r->oa[2 + ax] * pp->dsz + r->oo[2 + ax];
    double lo = fmin(c0, c1), hi = fmax(c0, c1);
    double d0 = pp->des0 + lo * pp->kp, d1 = pp->des0 + hi * pp->kp;
    if (pp->through && !ov) {
        if (anc && depth < K_MAX_DEPTH) {
            int ty = 1 - ax;
            double tu0 = r->oa[ty] * pp->vsz + r->oo[ty], tu1 = r->oa[2 + ty] * pp->vsz + r->oo[2 + ty];
            double ta0 = pp->act0 + (ca[ax] * pp->asz + co[ax]) * pp->kp;
            par_t tp;
            child_par(&tp, cv, pp, r, ca, co, d0, lo, hi, ta0, pp->v0 + fmin(tu0, tu1) * pp->kp, kpn);
            tp.through = 1;
            process_children(tf, cv, &tp, depth + 1);
        }
        return;
    }
    if (fabs(srel) < K_SCALE_MIN) { g_act[ACT_SCALED]++; return; }
    int fell = 0;
    if (fabs(srel - 1.0) > 0.01) {
        vec2 pv = FN(K_RT_GET_PIVOT, vec2, void *, const void *)(tf, NULL);
        double piv = d0 + (ax ? pv.y : pv.x) * (d1 - d0), sc = fabs(srel);
        double va = piv + (d0 - piv) * sc, vb = piv + (d1 - piv) * sc, x0 = 0.0, x1 = 0.0;
        double vlo = fmin(va, vb), vhi = fmax(va, vb);
        int sact = classify(vlo, vhi, cv, 0, &x0, &x1);
        double a0s = 0.0, a1s = 0.0;
        if (sact == ACT_STRETCH) fell = 1;
        else if (sact == ACT_LEFT || sact == ACT_RIGHT || sact == ACT_CENTER) {
            double sh = x0 - vlo;
            if (place_axis(tf, ax, pp->act0, pp->asz, pp->kp, d0 + sh, d1 + sh, &a0s, &a1s)) {
                rt_read(tf, ca, co);
                memcpy(r->aa, ca, sizeof r->aa);
                memcpy(r->ao, co, sizeof r->ao);
                r->keep = 1;
                g_changed++;
            }
            g_act[sact]++;
        } else g_act[ACT_SCALED]++;
        if (!fell && !r->logged && log_ok()) {
            char name[48];
            ascii_name(tf, name, sizeof name);
            r->logged = 1;
            g_log_lines++;
            LOG("[mod fullscreen-mode]   %*s\"%s\" scale %.2f, design %.0f..%.0f: %s", depth * 2, "", name, srel, vlo, vhi, k_act_name[sact == ACT_LEFT || sact == ACT_RIGHT || sact == ACT_CENTER ? sact : ACT_SCALED]);
        }
        if (!fell) return;
        g_act[ACT_SCALED]--;
    }
    double u0 = r->oa[ay] * pp->vsz + r->oo[ay];
    double u1 = r->oa[2 + ay] * pp->vsz + r->oo[2 + ay];
    double v0 = pp->v0 + fmin(u0, u1) * pp->kp, v1 = pp->v0 + fmax(u0, u1) * pp->kp;
    int nchild = -1, art = 0;
    if (d0 <= K_FULL_TOL && cv->design - d1 <= K_FULL_TOL && v0 <= K_FULL_TOL && cv->osize - v1 <= K_FULL_TOL) {
        nchild = child_count(tf);
        if (nchild == 0) {
            if (r->kind == 3 || r->kind == 4) art = r->kind == 3;
            else {
                art = !*bleed && depth <= K_ART_DEPTH;
                r->kind = art ? 3 : 4;
                r->keep = 1;
            }
            *bleed = 1;
        }
    }
    double t0 = 0.0, t1 = 0.0;
    int act = classify(d0, d1, cv, art, &t0, &t1);
    if (pp->grp && (act == ACT_LEFT || act == ACT_RIGHT)) {
        double gp = (cv->size - cv->design) / 2.0;
        t0 = d0 + gp;
        t1 = d1 + gp;
        act = ACT_CENTER;
    }
    int forced = ov && ov->mode != OV_AUTO;
    if (forced) act = ov_apply(ov->mode, d0, d1, cv, &t0, &t1);
    if (!forced && (act == ACT_LEFT || act == ACT_RIGHT || act == ACT_CENTER)) {
        if (nchild < 0) nchild = child_count(tf);
        act = settle_container(tf, r, cv, d0, d1, hi - lo, kpn, nchild, act, &t0, &t1);
    }
    if (ov && act != ACT_SKIP && act != ACT_THRU) { t0 += ov->dx; t1 += ov->dx; }
    g_act[act]++;
    double a0 = pp->act0 + (ca[ax] * pp->asz + co[ax]) * pp->kp;
    double a1 = pp->act0 + (ca[2 + ax] * pp->asz + co[2 + ax]) * pp->kp;
    if (act == ACT_SKIP) {
        if (!r->logged && log_ok()) {
            char name[48];
            ascii_name(tf, name, sizeof name);
            r->logged = 1;
            g_log_lines++;
            LOG("[mod fullscreen-mode]   %*s\"%s\" design %.0f..%.0f: %s", depth * 2, "", name, d0, d1, k_act_name[act]);
        }
        return;
    }
    double was0 = a0, was1 = a1;
    int changed = 0;
    if (act != ACT_THRU) {
        changed = place_axis(tf, ax, pp->act0, pp->asz, pp->kp, t0, t1, &a0, &a1);
        double dy = ov ? ov->dy : 0.0, b0, b1;
        if (act == ACT_COVER) {
            double f = cv->size / cv->design, mid = cv->osize / 2.0;
            changed |= place_axis(tf, ay, pp->v0, pp->vsz, pp->kp, mid + (v0 - mid) * f + dy, mid + (v1 - mid) * f + dy, &b0, &b1);
        } else if (ov) {
            changed |= place_axis(tf, ay, pp->v0, pp->vsz, pp->kp, v0 + dy, v1 + dy, &b0, &b1);
        }
        if (changed) {
            rt_read(tf, ca, co);
            memcpy(r->aa, ca, sizeof r->aa);
            memcpy(r->ao, co, sizeof r->ao);
            r->keep = 1;
            g_changed++;
        }
    }
    if (changed && r->logged < 2 && log_ok()) {
        char name[48];
        ascii_name(tf, name, sizeof name);
        r->logged++;
        g_log_lines++;
        LOG("[mod fullscreen-mode]   %*s\"%s\" %s: design %.0f..%.0f, was %.0f..%.0f, now %.0f..%.0f (screen %.0f)", depth * 2, "", name, k_act_name[act], d0, d1, was0, was1, a0, a1, cv->size);
    } else if (g_ui_level >= 3 && !r->vlog && log_ok()) {
        char name[48];
        ascii_name(tf, name, sizeof name);
        r->vlog = 1;
        g_log_lines++;
        LOG("[mod fullscreen-mode]   %*s\"%s\" %s: design %.0f..%.0f x %.0f..%.0f, now %.0f..%.0f", depth * 2, "", name, k_act_name[act], d0, d1, v0, v1, a0, a1);
    }
    int deep = act == ACT_STRETCH || act == ACT_THRU;
    int block = !deep || opaque(tf);
    if (depth >= K_MAX_DEPTH || (block && !anc)) return;
    par_t np;
    child_par(&np, cv, pp, r, ca, co, d0, lo, hi, a0, v0, kpn);
    np.through = block;
    process_children(tf, cv, &np, depth + 1);
}

static int group_centred(void *tf, const cv_t *cv, const par_t *pp, int n) {
    if (n < 2 || n > K_UNION_MAX) return 0;
    int ax = cv->ax, got = 0;
    double dw = cv->design, lo = 0.0, hi = 0.0, iv[K_UNION_MAX][2];
    for (int i = 0; i < n; i++) {
        void *child = child_at(tf, i);
        if (!child || (g_skip_inactive && !active_node(child))) continue;
        g_budget--;
        float ca[4], co[4];
        rt_read(child, ca, co);
        rec_t *cr = rec_find(child);
        int keep_a = cr && cr->seen && same4(ca, cr->aa, 1e-5f), keep_o = cr && cr->seen && same4(co, cr->ao, 1e-3f);
        const float *oa = keep_a ? cr->oa : ca, *oo = keep_o ? cr->oo : co;
        double c0 = oa[ax] * pp->dsz + oo[ax], c1 = oa[2 + ax] * pp->dsz + oo[2 + ax];
        double a = pp->des0 + fmin(c0, c1) * pp->kp, b = pp->des0 + fmax(c0, c1) * pp->kp;
        if (b - a >= K_BAND_FRAC * dw || b < -K_APPLY_EPS || a > dw + K_APPLY_EPS) continue;
        if (!got || a < lo) lo = a;
        if (!got || b > hi) hi = b;
        iv[got][0] = a;
        iv[got][1] = b;
        got++;
    }
    if (got < 2) return 0;
    for (int i = 1; i < got; i++) {
        double k0 = iv[i][0], k1 = iv[i][1];
        int j = i - 1;
        while (j >= 0 && iv[j][0] > k0) { iv[j + 1][0] = iv[j][0]; iv[j + 1][1] = iv[j][1]; j--; }
        iv[j + 1][0] = k0;
        iv[j + 1][1] = k1;
    }
    double reach = iv[0][1];
    for (int i = 1; i < got; i++) {
        if (iv[i][0] - reach > K_GROUP_GAP * dw) return 0;
        if (iv[i][1] > reach) reach = iv[i][1];
    }
    return lo >= K_GROUP_EDGE * dw && dw - hi >= K_GROUP_EDGE * dw && fabs((lo + hi) / 2.0 - dw / 2.0) <= K_GROUP_OFF * dw;
}

static void process_children(void *tf, const cv_t *cv, const par_t *pp0, int depth) {
    int n = child_count(tf), bleed = 0;
    par_t lp = *pp0;
    lp.grp = pp0->through ? 0 : group_centred(tf, cv, pp0, n);
    const par_t *pp = &lp;
    for (int i = 0; i < n && g_budget > 0; i++) {
        void *child = child_at(tf, i);
        if (!child || child == s_hl_tf || (g_skip_inactive && !active_node(child))) continue;
        s_idx = i;
        process_node(child, cv, pp, depth, &bleed);
    }
}

static int canvas_geom(void *sc, void *go, const float *ref, double aspect, int quiet, void **tfp, cv_t *cv, double *wcp, double *hcp) {
    void *tf = FN(K_COMPONENT_GET_TRANSFORM, void *, void *, const void *)(sc, NULL);
    if (!alive(tf)) return 0;
    if (!s_rt_klass) s_rt_klass = *(void *const *)tf;
    if (*(void *const *)tf != s_rt_klass) return 0;
    if (ref[0] < 1.0f || ref[1] < 1.0f) return 0;
    rect4 rc = FN(K_RT_GET_RECT, rect4, void *, const void *)(tf, NULL);
    double wc = rc.w, hc = rc.h;
    if (wc < 1.0 || hc < 1.0) return 0;
    if (fabs(wc / hc - aspect) > aspect * 0.02) {
        if (!quiet && first_note(sc) && log_ok()) {
            char name[48];
            ascii_name(go, name, sizeof name);
            g_log_lines++;
            LOG("[mod fullscreen-mode] canvas \"%s\" not laid out: rect %.0fx%.0f is not the screen's aspect %.3f", name, wc, hc, aspect);
        }
        return 0;
    }
    if (wc > ref[0] + 1.0) { cv->ax = 0; cv->size = wc; cv->design = ref[0]; cv->osize = hc; }
    else if (hc > ref[1] + 1.0) { cv->ax = 1; cv->size = hc; cv->design = ref[1]; cv->osize = wc; }
    else return 0;
    cv->cscale = lossy_scale(tf, cv->ax);
    if (cv->cscale < 1e-6) return 0;
    *tfp = tf;
    *wcp = wc;
    *hcp = hc;
    return 1;
}

static int layout_canvas(void *sc, void *go, const float *ref, double aspect) {
    void *tf = NULL;
    cv_t cv;
    double wc = 0.0, hc = 0.0;
    if (!canvas_geom(sc, go, ref, aspect, 0, &tf, &cv, &wc, &hc)) return 0;
    par_t pp = {0.0, cv.design, 0.0, cv.size, 1.0, 0.0, cv.osize, 0, 0};
    if (s_paths) {
        char root[K_SEG_MAX];
        canvas_root(go, root);
        s_par_h[0] = fnv(2166136261u, root);
    }
    int before = g_changed;
    process_children(tf, &cv, &pp, 0);
    if (g_changed != before && log_ok()) {
        char name[48];
        ascii_name(go, name, sizeof name);
        g_log_lines++;
        LOG("[mod fullscreen-mode] canvas \"%s\" %.0fx%.0f, reference %.0fx%.0f, %s axis: %d elements adjusted", name, wc, hc, ref[0], ref[1], cv.ax ? "vertical" : "horizontal", g_changed - before);
    }
    return 1;
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
    int scalers = 0, matched = 0, laid = 0, starved_at = -1;
    rec_compact();
    g_skip_inactive = g_sweep++ % K_FULL_EVERY != 0;
    g_budget = K_NODE_BUDGET;
    g_changed = 0;
    g_starved = 0;
    memset(g_act, 0, sizeof g_act);
    int start = n ? g_start % (int)n : 0;
    for (uint64_t k = 0; k < n; k++) {
        int i = (int)((start + k) % n);
        void *sc = *(void *const *)((uintptr_t)arr + F_ARRAY_DATA + 8 * (uint64_t)i);
        if (!alive(sc)) continue;
        void *go = FN(K_COMPONENT_GET_GAMEOBJECT, void *, void *, const void *)(sc, NULL);
        if (!alive(go) || !(FN(K_GO_ACTIVE_IN_HIERARCHY, uint8_t, void *, const void *)(go, NULL) & 1)) continue;
        scalers++;
        uint8_t *p = (uint8_t *)sc;
        int mode = *(int *)(p + F_SCALER_MODE);
        if (mode != SCALE_WITH_SCREEN_SIZE) {
            if (first_note(sc) && log_ok()) {
                char name[64];
                ascii_name(go, name, sizeof name);
                g_log_lines++;
                LOG("[mod fullscreen-mode] canvas \"%s\" not laid out: scaler mode %d is not scale with screen size", name, mode);
            }
            continue;
        }
        float *ref = (float *)(p + F_SCALER_REFERENCE);
        double ref_aspect = ref[0] > 0.0f && ref[1] > 0.0f ? (double)ref[0] / ref[1] : K_DESIGN_ASPECT;
        float want = aspect >= ref_aspect ? 1.0f : 0.0f;
        int mm = *(int *)(p + F_SCALER_MATCH_MODE);
        float match = *(float *)(p + F_SCALER_MATCH);
        if (mm != MATCH_WIDTH_OR_HEIGHT || !near(match, want)) {
            if (log_ok()) {
                char name[64];
                ascii_name(go, name, sizeof name);
                g_log_lines++;
                LOG("[mod fullscreen-mode] canvas \"%s\": scaler reference %.0fx%.0f, match mode %d, match %.2f -> match %.0f", name, ref[0], ref[1], mm, match, want);
            }
            *(int *)(p + F_SCALER_MATCH_MODE) = MATCH_WIDTH_OR_HEIGHT;
            *(float *)(p + F_SCALER_MATCH) = want;
            matched++;
        }
        int was_starved = g_starved;
        if (level >= 2 && layout_canvas(sc, go, ref, aspect)) laid++;
        if (g_starved && !was_starved && starved_at < 0) starved_at = i;
    }
    g_start = starved_at < 0 ? 0 : (starved_at == start ? start + 1 : starved_at);
    static int starved_logged;
    if (g_starved && starved_logged++ < 20) LOG("[mod fullscreen-mode] frame %d: node budget %d used up at canvas %d of %d, the next sweep resumes there", g_frame, K_NODE_BUDGET, starved_at, (int)n);
    static int logged;
    if ((matched || g_changed) && logged++ < 40)
        LOG("[mod fullscreen-mode] frame %d: %d of %d canvas scalers set to match, %d canvases laid out (screen %dx%d): %d elements adjusted of %d seen (%d left, %d right, %d centred, %d stretched, %d expanded, %d containers passed, %d left alone)", g_frame, matched, scalers, laid, sw, sh, g_changed, g_act[ACT_LEFT] + g_act[ACT_RIGHT] + g_act[ACT_CENTER] + g_act[ACT_STRETCH] + g_act[ACT_COVER] + g_act[ACT_THRU] + g_act[ACT_SKIP] + g_act[ACT_SCALED], g_act[ACT_LEFT], g_act[ACT_RIGHT], g_act[ACT_CENTER], g_act[ACT_STRETCH], g_act[ACT_COVER], g_act[ACT_THRU], g_act[ACT_SKIP] + g_act[ACT_SCALED]);
}

static int edit_enabled(void) {
    if (s_edit < 0) {
        const char *v = getenv("DM_MOD_FULLSCREEN_EDIT");
        s_edit = v && v[0] == '1';
        if (s_edit) s_paths = 1;
        LOG("[mod fullscreen-mode] layout editor %s (DM_MOD_FULLSCREEN_EDIT=%s): the special button toggles select mode, tap an element to select it, tap the same spot or ZL / ZR to step through the elements under it, L / R change its placement (auto, left, centre, right, stretch, expand), the D-pad nudges it, Y resets it, B deselects, X saves %s", s_edit ? "on" : "off", v ? v : "unset", layout_path());
    }
    return s_edit;
}

static int pick_cmp(const void *a, const void *b) {
    const pick_t *x = a, *y = b;
    if (x->area < y->area) return -1;
    if (x->area > y->area) return 1;
    return y->depth - x->depth;
}

static void pick_walk(void *tf, const cv_t *cv, const pk_t *p, int depth, uint32_t ph, double px, double py) {
    if (depth >= K_PICK_DEPTH) return;
    int n = child_count(tf);
    for (int i = 0; i < n && g_budget > 0; i++) {
        void *child = child_at(tf, i);
        if (!child || child == s_hl_tf || !active_node(child)) continue;
        g_budget--;
        float ca[4], co[4];
        rt_read(child, ca, co);
        double xa = p->x0 + (ca[0] * p->xs + co[0]) * p->kp, xb = p->x0 + (ca[2] * p->xs + co[2]) * p->kp;
        double ya = p->y0 + (ca[1] * p->ys + co[1]) * p->kp, yb = p->y0 + (ca[3] * p->ys + co[3]) * p->kp;
        double x0 = fmin(xa, xb), x1 = fmax(xa, xb), y0 = fmin(ya, yb), y1 = fmax(ya, yb);
        seg_make(child, i, s_pseg[depth + 1]);
        uint32_t h = fnv(fnv(ph, "/"), s_pseg[depth + 1]);
        if (px >= x0 && px <= x1 && py >= y0 && py <= y1 && x1 - x0 > 1.0 && y1 - y0 > 1.0 && s_pick_n < K_PICK_MAX) {
            pick_t *k = &s_pick[s_pick_n];
            size_t len = 0;
            int ok = 1;
            k->key[0] = 0;
            for (int d = 0; d <= depth + 1 && ok; d++) {
                int w = snprintf(k->key + len, sizeof k->key - len, d ? "/%s" : "%s", s_pseg[d]);
                if (w < 0 || len + (size_t)w >= sizeof k->key) ok = 0;
                else len += (size_t)w;
            }
            if (ok) {
                k->tf = child;
                k->h = h;
                k->depth = depth;
                k->area = (x1 - x0) * (y1 - y0);
                s_pick_n++;
            }
        }
        pk_t np = {xa < xb ? xa : xb, ((double)ca[2] - ca[0]) * p->xs + ((double)co[2] - co[0]), ya < yb ? ya : yb, ((double)ca[3] - ca[1]) * p->ys + ((double)co[3] - co[1]), 1.0};
        double sc = lossy_scale(child, cv->ax);
        np.kp = cv->cscale > 1e-6 && sc > 1e-6 ? sc / cv->cscale : 1.0;
        if (ca[0] > ca[2] || ca[1] > ca[3]) { np.x0 = xa; np.y0 = ya; }
        pick_walk(child, cv, &np, depth + 1, h, px, py);
    }
}

static int pick_scan(float nx, float ny) {
    s_pick_n = 0;
    if (!resolve_types()) return 0;
    int sw = FN(K_SCREEN_GET_WIDTH, int, const void *)(NULL), sh = FN(K_SCREEN_GET_HEIGHT, int, const void *)(NULL);
    if (sw <= 0 || sh <= 0) return 0;
    double aspect = (double)sw / sh;
    void *arr = FN(K_RESOURCES_FINDALL, void *, void *, const void *)(g_scaler_type, NULL);
    uint64_t n = arr ? *(const uint64_t *)((uintptr_t)arr + F_ARRAY_LEN) : 0;
    g_budget = K_NODE_BUDGET * 2;
    for (uint64_t i = 0; i < n; i++) {
        void *sc = *(void *const *)((uintptr_t)arr + F_ARRAY_DATA + 8 * i);
        if (!alive(sc)) continue;
        void *go = FN(K_COMPONENT_GET_GAMEOBJECT, void *, void *, const void *)(sc, NULL);
        if (!alive(go) || !(FN(K_GO_ACTIVE_IN_HIERARCHY, uint8_t, void *, const void *)(go, NULL) & 1)) continue;
        uint8_t *q = (uint8_t *)sc;
        if (*(int *)(q + F_SCALER_MODE) != SCALE_WITH_SCREEN_SIZE) continue;
        void *tf = NULL;
        cv_t cv;
        double wc = 0.0, hc = 0.0;
        if (!canvas_geom(sc, go, (const float *)(q + F_SCALER_REFERENCE), aspect, 1, &tf, &cv, &wc, &hc)) continue;
        canvas_root(go, s_pseg[0]);
        pk_t root = {0.0, wc, 0.0, hc, 1.0};
        pick_walk(tf, &cv, &root, 0, fnv(2166136261u, s_pseg[0]), (double)nx * wc, (1.0 - (double)ny) * hc);
    }
    qsort(s_pick, (size_t)s_pick_n, sizeof s_pick[0], pick_cmp);
    return s_pick_n;
}

static void hl_clear(void) {
    if (s_hl_img && alive(s_hl_img)) {
        void *go = FN(K_COMPONENT_GET_GAMEOBJECT, void *, void *, const void *)(s_hl_img, NULL);
        if (alive(go)) FN(K_OBJECT_DESTROY, void, void *, const void *)(go, NULL);
    }
    s_hl_img = s_hl_tf = NULL;
}

static void *hl_source(void) {
    if (s_hl_src && alive(s_hl_src)) return s_hl_src;
    s_hl_src = NULL;
    if (!g_image_type) return NULL;
    void *arr = FN(K_RESOURCES_FINDALL, void *, void *, const void *)(g_image_type, NULL);
    uint64_t n = arr ? *(const uint64_t *)((uintptr_t)arr + F_ARRAY_LEN) : 0;
    for (uint64_t i = 0; i < n && !s_hl_src; i++) {
        void *img = *(void *const *)((uintptr_t)arr + F_ARRAY_DATA + 8 * i);
        if (!alive(img)) continue;
        void *tf = FN(K_COMPONENT_GET_TRANSFORM, void *, void *, const void *)(img, NULL);
        if (!alive(tf) || *(void *const *)tf != s_rt_klass || !active_node(tf) || child_count(tf) != 0) continue;
        s_hl_src = img;
    }
    return s_hl_src;
}

static int sel_mode(void) {
    const ov_t *o = ov_find(s_sel_h);
    return o ? o->mode : OV_AUTO;
}

static void hl_make(void) {
    hl_clear();
    if (!s_sel_tf || !alive(s_sel_tf)) return;
    void *src = hl_source();
    if (!src) { LOG("[mod fullscreen-mode] edit: no image to build the highlight from, the selection is only logged"); return; }
    void *img = FN(K_OBJECT_INSTANTIATE_PARENT, void *, void *, void *, const void *)(src, s_sel_tf, NULL);
    if (!alive(img)) return;
    void *tf = FN(K_COMPONENT_GET_TRANSFORM, void *, void *, const void *)(img, NULL);
    if (!alive(tf)) return;
    float a[4] = {0.0f, 0.0f, 1.0f, 1.0f}, o[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    vec3 one = {1.0f, 1.0f, 1.0f};
    FN(K_IMAGE_SET_SPRITE, void, void *, void *, const void *)(img, NULL, NULL);
    FN(K_GRAPHIC_SET_RAYCAST, void, void *, int, const void *)(img, 0, NULL);
    FN(K_GRAPHIC_SET_COLOR, void, void *, rgba, const void *)(img, k_ov_color[sel_mode()], NULL);
    rt_write_anchors(tf, a);
    rt_write_offsets(tf, o);
    FN(K_TRANSFORM_SET_LOCALSCALE, void, void *, vec3, const void *)(tf, one, NULL);
    FN(K_TRANSFORM_SET_AS_LAST_SIBLING, void, void *, const void *)(tf, NULL);
    s_hl_img = img;
    s_hl_tf = tf;
}

static void sel_set(const pick_t *k) {
    s_sel_tf = k->tf;
    s_sel_h = k->h;
    snprintf(s_sel_key, sizeof s_sel_key, "%s", k->key);
    hl_make();
    const ov_t *o = ov_find(s_sel_h);
    LOG("[mod fullscreen-mode] edit: selected %d of %d \"%s\": %s, dx %.0f, dy %.0f", s_pick_i + 1, s_pick_n, s_sel_key, k_ov_name[o ? o->mode : OV_AUTO], o ? o->dx : 0.0f, o ? o->dy : 0.0f);
}

static void sel_clear(void) {
    hl_clear();
    s_sel_tf = NULL;
    s_sel_h = 0;
    s_sel_key[0] = 0;
    s_pick_n = 0;
}

static void edit_changed(const ov_t *o) {
    if (o) {
        if (s_hl_img && alive(s_hl_img)) FN(K_GRAPHIC_SET_COLOR, void, void *, rgba, const void *)(s_hl_img, k_ov_color[o->mode], NULL);
        LOG("[mod fullscreen-mode] edit: \"%s\" %s, dx %.0f, dy %.0f", s_sel_key, k_ov_name[o->mode], o->dx, o->dy);
    }
    g_next_ui = g_frame;
}

static ov_t *sel_ov(void) {
    ov_t *o = ov_find(s_sel_h);
    return o ? o : ov_make(s_sel_key);
}

static void sel_tidy(ov_t *o) {
    if (o && o->mode == OV_AUTO && o->dx == 0.0f && o->dy == 0.0f) {
        ov_drop(o);
        edit_changed(NULL);
        if (s_hl_img && alive(s_hl_img)) FN(K_GRAPHIC_SET_COLOR, void, void *, rgba, const void *)(s_hl_img, k_ov_color[OV_AUTO], NULL);
        LOG("[mod fullscreen-mode] edit: \"%s\" back to auto", s_sel_key);
    }
}

static void edit_tap(float nx, float ny) {
    int n = pick_scan(nx, ny);
    if (n <= 0) {
        LOG("[mod fullscreen-mode] edit: nothing found at %.3f, %.3f", nx, ny);
        sel_clear();
        s_last_nx = -1.0f;
        return;
    }
    int idx = 0;
    int same = s_sel_h && hypotf(nx - s_last_nx, ny - s_last_ny) < K_SAME_SPOT;
    if (same)
        for (int i = 0; i < n; i++)
            if (s_pick[i].h == s_sel_h) { idx = (i + 1) % n; break; }
    s_last_nx = nx;
    s_last_ny = ny;
    s_pick_i = idx;
    sel_set(&s_pick[idx]);
}

static void edit_step(int dir) {
    if (s_pick_n <= 0) return;
    s_pick_i = (s_pick_i + s_pick_n + dir) % s_pick_n;
    sel_set(&s_pick[s_pick_i]);
}

static void edit_nudge(uint64_t keys) {
    static const uint64_t bits[4] = {DM_BTN_LEFT, DM_BTN_RIGHT, DM_BTN_UP, DM_BTN_DOWN};
    float d[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    int fire = 0;
    for (int i = 0; i < 4; i++) {
        s_hold[i] = (keys & bits[i]) ? s_hold[i] + 1 : 0;
        int h = s_hold[i];
        if (h == 1 || (h > 20 && h % 2 == 0)) {
            d[i] = h > 90 ? 8.0f : h > 45 ? 4.0f : 1.0f;
            fire = 1;
        }
    }
    if (!fire) return;
    ov_t *o = sel_ov();
    if (!o) return;
    o->dx += d[1] - d[0];
    o->dy += d[2] - d[3];
    if (o->dx == 0.0f && o->dy == 0.0f && o->mode == OV_AUTO) { sel_tidy(o); return; }
    edit_changed(o);
}

static void edit_run(void) {
    if (!edit_enabled() || g_state != 1 || !g_applied) return;
    int frame = FN(K_TIME_FRAMECOUNT, int, const void *)(NULL);
    uint64_t keys = dm_mod_keys(), down = keys & ~s_prev_keys;
    s_prev_keys = keys;
    dm_mouse_frame_t mf;
    dm_mouse_frame((uint64_t)frame, &mf);
    if (down & dm_mod_special()) {
        s_select = !s_select;
        if (!s_select) sel_clear();
        LOG("[mod fullscreen-mode] edit: select mode %s", s_select ? "on" : "off");
        return;
    }
    if (!s_select) return;
    dm_mod_input_mask(DM_BTN_LEFT | DM_BTN_RIGHT | DM_BTN_UP | DM_BTN_DOWN | DM_BTN_X | DM_BTN_Y | DM_BTN_B | DM_BTN_L | DM_BTN_R | DM_BTN_ZL | DM_BTN_ZR, 120);
    if (s_hl_img && !alive(s_hl_img)) hl_clear();
    if (s_sel_tf && !alive(s_sel_tf)) sel_clear();
    if (mf.down & DM_MOUSE_LEFT) { edit_tap(mf.nx, mf.ny); return; }
    if (down & DM_BTN_X) { layout_save(); return; }
    if (!s_sel_tf) return;
    if (down & DM_BTN_B) { sel_clear(); return; }
    if (down & DM_BTN_ZL) { edit_step(-1); return; }
    if (down & DM_BTN_ZR) { edit_step(1); return; }
    if (down & (DM_BTN_L | DM_BTN_R)) {
        ov_t *o = sel_ov();
        if (!o) return;
        o->mode = (uint8_t)((o->mode + OV_COUNT + ((down & DM_BTN_R) ? 1 : -1)) % OV_COUNT);
        if (o->mode == OV_AUTO && o->dx == 0.0f && o->dy == 0.0f) sel_tidy(o);
        else edit_changed(o);
        return;
    }
    if (down & DM_BTN_Y) {
        ov_t *o = ov_find(s_sel_h);
        if (o) { o->mode = OV_AUTO; o->dx = o->dy = 0.0f; sel_tidy(o); }
        return;
    }
    edit_nudge(keys);
}

static void frame_run(uint64_t *r, int param) {
    (void)r; (void)param;
    if (g_state < 0 || !main_base()) return;
    if (g_state == 0) {
        if (strncmp(dm_main_build(), K_BUILD_ID, 32) != 0) { g_state = -1; LOG("[mod fullscreen-mode] wrong build, mod stays out"); return; }
        g_state = 1;
        edit_enabled();
        if (!s_loaded) { s_loaded = 1; layout_load(); }
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
    edit_run();
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
