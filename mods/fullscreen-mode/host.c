// Based on Bat'ko Makhno's Resolution Fix (Steam Workshop 2893583716)
#include "dm.h"
#include "front.h"
#include "hle_mods_gen.h"
#include <math.h>
#include <pthread.h>

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
#define K_TRANSFORM_GET_PARENT 0x19BC440u
#define K_TRANSFORM_GET_SIBLING 0x19BF870u
#define K_TRANSFORM_GET_POSITION 0x19BC6B0u
#define K_TRANSFORM_SET_POSITION 0x19BC780u
#define K_TRANSFORM_GET_LOCALSCALE 0x19BD6C0u
#define K_RENDERER_GET_BOUNDS 0x17FA410u
#define K_RENDERER_GET_ENABLED 0x17FAA60u
#define K_CAMERA_W2S 0x17EC830u
#define K_CAMERA_GET_ALL 0x17EE210u
#define K_CAMERA_GET_MASK 0x17E9BE0u
#define K_CAMERA_GET_DEPTH 0x17E9940u
#define K_CAMERA_GET_ORTHO 0x17E9510u
#define K_CAMERA_GET_PW 0x17EB4B0u
#define K_CAMERA_GET_PH 0x17EB500u
#define K_CAMERA_GET_TEXTURE 0x17EB5F0u
#define K_GO_GET_LAYER 0x19AFBF0u
#define K_TRANSFORM_GET_LOSSY_SCALE 0x19BFA20u
#define K_IMAGE_GET_OVERRIDE_SPRITE 0x24E57E0u
#define K_SPRITE_GET_TEXTURE 0x19C1BD0u
#define K_RAWIMAGE_GET_TEXTURE 0x2969270u
#define K_TEXTURE_GET_WIDTH 0x2BE3930u
#define K_TEXTURE_GET_HEIGHT 0x2BE39D0u
#define K_TEX2D_WHITE 0x2BE48E0u
#define K_OBJECT_GETTYPE 0x29B10F0u
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
#define K_NEAR_FRAC 0.10
#define K_STRETCH_MARGIN 0.08
#define K_POINT_FRAC 0.05
#define K_GROUP_EDGE 0.06
#define K_GROUP_OFF 0.20
#define K_GROUP_GAP 0.08
#define K_CG_MIN_W 960
#define K_CG_MIN_H 540
#define K_CG_ASPECT_MIN 1.5
#define K_CG_ASPECT_MAX 2.4
#define K_SCALE_MIN 0.05
#define K_APPLY_EPS 0.5
#define K_DESIGN_ASPECT (16.0 / 9.0)
#define K_OV_MAX 512
#define K_TAB 4096
#define K_KEY_MAX 2048
#define K_SEG_MAX 48
#define K_PICK_DEPTH 64
#define K_PICK_BUDGET 4000000
#define K_WORLD_RETRY 120
#define K_WORLD_EPS 0.0001f
#define K_SAME_SPOT 0.012f
#define K_HL_ALPHA 0.4f
#define K_LAYOUT_FILE "fullscreen_layout.txt"
#define K_BAKE_VER 2
#define K_BAKE_FILE "fullscreen_baked.txt"
#define K_BAKE_SAVE_EVERY 600
#define K_BAKE_MISS_MAX 3
#define K_BAKE_MAX (1 << 20)
#define K_UI_EVERY_BAKED 8

typedef struct { float x, y; } vec2;
typedef struct { float x, y, z; } vec3;
typedef struct { float x, y, w, h; } rect4;
typedef struct { void *obj, *tex; float oa[4], oo[4], aa[4], ao[4], base[4], sh; uint32_t kh; uint8_t seen, logged, vlog, keep, kind, act, cover, calc, cact; } rec_t;
typedef struct { int ax; double size, design, osize, cscale; } cv_t;
typedef struct { double des0, dsz, act0, asz, kp, v0, vsz; int grp, through, grp_ok, n; void *ptf; } par_t;
typedef struct { float r, g, b, a; } rgba;
typedef struct { uint32_t h; float wc, oa[4], oo[4], fa[4], fo[4], pc[5]; uint8_t act, placed, block, miss; } bk_t;
typedef struct { double x0, xs, y0, ys, kp; } pk_t;
typedef struct { uint32_t h; uint8_t mode; float dx, dy; char key[K_KEY_MAX]; } ov_t;
typedef struct { uint32_t h; int16_t ov; uint8_t anc; } slot_t;
typedef struct { void *tf, *rend, *cam; uint32_t h; int depth, world; double area; char key[K_KEY_MAX]; } pick_t;
typedef struct { float x, y, z, ex, ey, ez; } bounds_t;
typedef struct { void *tf, *rend, *cam; int have, pw, ph, calc; vec3 bp, bs, ap, as; bounds_t bb; double base[4]; } wld_t;

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
static int s_ov_n, s_paths, s_idx, s_edit = -1, s_loaded, s_pick_n, s_pick_i;
static uint32_t s_par_h[K_MAX_DEPTH + 4], s_sel_h;
static char s_pseg[K_PICK_DEPTH + 2][K_SEG_MAX], s_sel_key[K_KEY_MAX];
static pick_t *s_pick;
static int s_pick_cap, s_sel_world, s_wld_next;
static double s_unit = 1.0;
static wld_t s_wld[K_OV_MAX];
static void *s_sel_tf, *s_sel_rend, *s_sel_cam, *s_ui_root, *s_hl_img, *s_hl_tf, *s_hl_src, *g_image_type, *g_rend_type, *g_raw_type, *g_tex_type;
static int g_rend_tries;
static float s_last_nx = -1.0f, s_last_ny = -1.0f;
typedef struct { int kind; float a, b; } fs_cmd_t;
static fs_cmd_t s_q[64];
static int s_qh, s_qt;
static pthread_mutex_t s_qm = PTHREAD_MUTEX_INITIALIZER;

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
    if (g_scaler_type && g_layout_type && g_scroll_type && g_image_type && g_raw_type && g_tex_type) return 1;
    if (g_type_tries < K_TYPE_TRIES) {
        g_type_tries++;
        if (!g_scaler_type) g_scaler_type = find_type("UnityEngine.UI.CanvasScaler, UnityEngine.UI");
        if (!g_layout_type) g_layout_type = find_type("UnityEngine.UI.LayoutGroup, UnityEngine.UI");
        if (!g_scroll_type) g_scroll_type = find_type("UnityEngine.UI.ScrollRect, UnityEngine.UI");
        if (!g_image_type) g_image_type = find_type("UnityEngine.UI.Image, UnityEngine.UI");
        if (!g_raw_type) g_raw_type = find_type("UnityEngine.UI.RawImage, UnityEngine.UI");
        if (!g_tex_type) {
            void *white = FN(K_TEX2D_WHITE, void *, const void *)(NULL);
            if (alive(white)) g_tex_type = FN(K_OBJECT_GETTYPE, void *, void *, const void *)(white, NULL);
        }
        LOG("[mod fullscreen-mode] types: CanvasScaler %p, LayoutGroup %p, ScrollRect %p, Image %p, RawImage %p, Texture2D %p", g_scaler_type, g_layout_type, g_scroll_type, g_image_type, g_raw_type, g_tex_type);
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

static void calc_set(rec_t *r, int calc, int act, double x0, double x1, double y0, double y1) {
    r->calc = (uint8_t)calc;
    r->cact = (uint8_t)act;
    r->base[0] = (float)x0;
    r->base[1] = (float)x1;
    r->base[2] = (float)y0;
    r->base[3] = (float)y1;
}

static int classify(double d0, double d1, const cv_t *cv, int art, double *t0, double *t1) {
    double dw = cv->design, wc = cv->size, pad = (wc - dw) / 2.0, mid = dw / 2.0;
    double w = d1 - d0, ml = d0, mr = dw - d1;
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
    if (ml <= K_STRETCH_MARGIN * dw && mr <= K_STRETCH_MARGIN * dw) {
        *t0 = ml;
        *t1 = wc - mr;
        if (*t1 < *t0) *t1 = *t0;
        return ACT_STRETCH;
    }
    double reach = K_NEAR_FRAC * dw, sh = pad;
    int act = ACT_CENTER;
    if (ml <= reach && mr > reach) { sh = 0.0; act = ACT_LEFT; }
    else if (mr <= reach && ml > reach) { sh = 2.0 * pad; act = ACT_RIGHT; }
    *t0 = d0 + sh;
    *t1 = d1 + sh;
    return act;
}

static int opaque(void *tf) {
    if (g_layout_type && alive(FN(K_COMPONENT_GET_COMPONENT, void *, void *, void *, const void *)(tf, g_layout_type, NULL))) return 1;
    if (g_scroll_type && alive(FN(K_COMPONENT_GET_COMPONENT, void *, void *, void *, const void *)(tf, g_scroll_type, NULL))) return 1;
    return 0;
}

static void *graphic_texture(void *tf) {
    void *tex = NULL;
    if (g_image_type) {
        void *img = FN(K_COMPONENT_GET_COMPONENT, void *, void *, void *, const void *)(tf, g_image_type, NULL);
        void *spr = alive(img) ? FN(K_IMAGE_GET_OVERRIDE_SPRITE, void *, void *, const void *)(img, NULL) : NULL;
        if (alive(spr)) tex = FN(K_SPRITE_GET_TEXTURE, void *, void *, const void *)(spr, NULL);
    }
    if (!alive(tex) && g_raw_type && g_tex_type) {
        void *raw = FN(K_COMPONENT_GET_COMPONENT, void *, void *, void *, const void *)(tf, g_raw_type, NULL);
        tex = alive(raw) ? FN(K_RAWIMAGE_GET_TEXTURE, void *, void *, const void *)(raw, NULL) : NULL;
        if (alive(tex) && FN(K_OBJECT_GETTYPE, void *, void *, const void *)(tex, NULL) != g_tex_type) tex = NULL;
    }
    return alive(tex) ? tex : NULL;
}

static int cg_texture(void *tex, int *w, int *h) {
    *w = FN(K_TEXTURE_GET_WIDTH, int, void *, const void *)(tex, NULL);
    *h = FN(K_TEXTURE_GET_HEIGHT, int, void *, const void *)(tex, NULL);
    if (*w < K_CG_MIN_W || *h < K_CG_MIN_H) return 0;
    double aspect = (double)*w / *h;
    return aspect >= K_CG_ASPECT_MIN && aspect <= K_CG_ASPECT_MAX;
}

static int node_art(void *tf, rec_t *r) {
    void *tex = graphic_texture(tf);
    if ((r->kind == 3 || r->kind == 4) && r->tex == tex) return r->kind == 3;
    int w = 0, h = 0, art = tex && cg_texture(tex, &w, &h);
    r->tex = tex;
    r->kind = art ? 3 : 4;
    r->keep = 1;
    if ((art || g_ui_level >= 3) && log_ok()) {
        char name[48];
        ascii_name(tf, name, sizeof name);
        g_log_lines++;
        if (tex) LOG("[mod fullscreen-mode]   \"%s\" texture %dx%d: %s", name, w, h, art ? "CG, expanded to cover" : "not a CG, stretched");
        else LOG("[mod fullscreen-mode]   \"%s\" no sprite or texture: stretched", name);
    }
    return art;
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

static int is_clone_name(const char *name) {
    size_t n = strlen(name);
    return n >= 7 && !strcmp(name + n - 7, "(Clone)");
}

static void seg_make(void *tf, int idx, char *out) {
    char name[K_SEG_MAX - 8];
    ascii_name(tf, name, sizeof name);
    seg_clean(name);
    if (is_clone_name(name)) snprintf(out, K_SEG_MAX, "%s[*]", name);
    else snprintf(out, K_SEG_MAX, "%s[%d]", name, idx);
}

static void key_canon(const char *in, char *out, size_t cap) {
    size_t o = 0;
    while (*in && o + 1 < cap) {
        if (!strncmp(in, "(Clone)[", 8)) {
            const char *p = in + 8;
            while (*p >= '0' && *p <= '9') p++;
            if (p > in + 8 && *p == ']') {
                for (const char *rep = "(Clone)[*]"; *rep && o + 1 < cap; rep++) out[o++] = *rep;
                in = p + 1;
                continue;
            }
        }
        out[o++] = *in++;
    }
    out[o] = 0;
}

static void canvas_root(void *go, char *out) {
    char name[K_SEG_MAX - 4];
    ascii_name(go, name, sizeof name);
    seg_clean(name);
    snprintf(out, K_SEG_MAX, "c:%s", name);
}

static uint32_t key_hash(const char *key, int register_ancestors) {
    char buf[K_KEY_MAX];
    key_canon(key, buf, sizeof buf);
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
    char canon[K_KEY_MAX];
    key_canon(key, canon, sizeof canon);
    uint32_t h = key_hash(canon, 1);
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
    snprintf(o->key, sizeof o->key, "%s", canon);
    sl->ov = (int16_t)idx;
    s_paths = 1;
    return o;
}

static void world_release(int idx);

static void ov_drop(ov_t *o) {
    world_release((int)(o - s_ov));
    slot_t *sl = slot_get(o->h, 0);
    if (sl) sl->ov = -1;
    memset(o, 0, sizeof *o);
}

static const char *layout_path(void) {
    const char *v = getenv("DM_MOD_FULLSCREEN_LAYOUT");
    return v && v[0] ? v : K_LAYOUT_FILE;
}

typedef struct { const char *key; uint8_t mode; float dx, dy; } layout_default_t;

static const layout_default_t k_layout_default[] = {
    {"c:TitleUICanvas/_Root_RuinTitle[0]/_Rect_ZoomPivot[3]/_Rect_TitleLogo[0]/_Rect_Buttons[4]/_Image_BlackFrame[3]", OV_STRETCH, 0.0f, 0.0f},
    {"c:TitleUICanvas/_Root_RuinTitle[0]/_Rect_ZoomPivot[3]/_Rect_TitleLogo[0]/_Rect_Buttons[4]/_Image_ExitBG[1]", OV_RIGHT, 0.0f, 0.0f},
    {"c:TitleUICanvas/_Root_RuinTitle[0]/_Rect_ZoomPivot[3]/_Rect_TitleLogo[0]/_Rect_Buttons[4]", OV_RIGHT, 0.0f, 0.0f},
    {"c:FrontCanvas/Librarian_Info_Panel(Clone)[11]/PanelActiveController[0]/_Rect_EquipCardSlotView[4]/_Button_CustomSelectableGraphic[0]", OV_CENTER, 0.0f, 0.0f},
    {"c:FrontCanvas/Librarian_Info_Panel(Clone)[11]/PanelActiveController[0]/_Rect_EquipCardSlotView[4]", OV_CENTER, 0.0f, 0.0f},
    {"c:Canvas_Script_PopupCanvas(Clone)/_Prefab_PopupAlarm[3]/_Rect_Blue[3]/_Image_BgFrame[1]", OV_STRETCH, 0.0f, 0.0f},
    {"c:FrontCanvas/Popup[4]/_Popup_PopupPassiveSuccessionPopup[4]", OV_STRETCH, 0.0f, 0.0f},
    {"c:FrontCanvas/Librarian_Info_Panel(Clone)[12]/PanelActiveController[0]/_Rect_EquipCardSlotView[4]", OV_CENTER, 0.0f, 0.0f},
    {"c:FrontCanvas/Popup[4]/_Popup_GachaResultPopup[5]/PanelActiveController[0]/Center_Panel[0]/Background[2]", OV_STRETCH, 0.0f, 0.0f},
    {"c:FrontCanvas/Popup[4]/_Popup_GachaResultPopup[5]/PanelActiveController[0]/Center_Panel[0]/BlackBg[1]", OV_STRETCH, 0.0f, 0.0f},
    {"c:Canvas_UI/_Rect_Buttons[8]/FastButton[1]", OV_LEFT, 0.0f, 0.0f},
    {"c:Canvas_UI/_Rect_Buttons[8]/FastButton[1]/_Image_Icon[0]", OV_LEFT, 0.0f, 0.0f},
    {"c:_Canvas_EmotionInfoUI/_Transform_EmotionBar_ROOT[2]/_Rect_CenterRoot[2]/Btn[10]", OV_CENTER, 0.0f, 0.0f},
    {"c:_Canvas_EmotionInfoUI/_Transform_EmotionBar_ROOT[2]/_Rect_CenterRoot[2]/_Image_Left_Lv2_LinearDodge[5]", OV_CENTER, 0.0f, 0.0f},
    {"c:_Canvas_EmotionInfoUI/_Transform_EmotionBar_ROOT[2]/_Rect_CenterRoot[2]/_Image_Right_Lv1_LinearDodge[6]", OV_CENTER, 0.0f, 0.0f},
    {"c:_Canvas_EmotionInfoUI/_Transform_EmotionBar_ROOT[2]/_Rect_CenterRoot[2]/_Image_Left_Lv1_LinearDodge[7]", OV_CENTER, 0.0f, 0.0f},
    {"c:_Canvas_EmotionInfoUI/_Transform_EmotionBar_ROOT[2]/_Rect_CenterRoot[2]/_Image_Right_Lv2_LinearDodge[4]", OV_CENTER, 0.0f, 0.0f},
    {"c:_Canvas_EmotionInfoUI/_Transform_EmotionBar_ROOT[2]/_Rect_CenterRoot[2]/_Image_Right_Lv1[3]", OV_CENTER, 0.0f, 0.0f},
    {"c:_Canvas_EmotionInfoUI/_Transform_EmotionBar_ROOT[2]/_Rect_CenterRoot[2]/_Image_Right_Lv2[1]", OV_CENTER, 0.0f, 0.0f},
    {"c:_Canvas_EmotionInfoUI/_Transform_EmotionBar_ROOT[2]/_Rect_CenterRoot[2]/_Image_Left_Lv2[0]", OV_CENTER, 0.0f, 0.0f},
    {"c:_Canvas_EmotionInfoUI/_Transform_EmotionBar_ROOT[2]/_Rect_CenterRoot[2]/_Image_Left_Lv1[2]", OV_CENTER, 0.0f, 0.0f},
    {"c:_Canvas_EmotionInfoUI/_Transform_EmotionBar_ROOT[2]/_Rect_CenterRoot[2]/_Rect_TargetCheckToggles[16]/_Image_Bg[0]", OV_AUTO, -128.0f, 0.0f},
    {"c:_Canvas_EmotionInfoUI/_Transform_EmotionBar_ROOT[2]/_Rect_CenterRoot[2]/_Image_AutoEquipCard[13]", OV_AUTO, 64.0f, 0.0f},
    {"c:_Canvas_EmotionInfoUI/_Transform_EmotionBar_ROOT[2]/_Rect_CenterRoot[2]/_Image_PlayAuto[12]/_Image_BG[1]", OV_AUTO, 80.0f, 0.0f},
    {"c:_Canvas_EmotionInfoUI/_Transform_EmotionBar_ROOT[2]/_Rect_CenterRoot[2]/_Image_UnequipCardAll[14]", OV_AUTO, 64.0f, 0.0f},
    {"c:FrontCanvas/Librarian_Info_Panel(Clone)[15]/PanelActiveController[0]/_Rect_EquipCardSlotView[4]", OV_CENTER, 0.0f, 0.0f},
    {"c:FrontCanvas/Panel_Invitation_Panel(Clone)[9]/PanelActiveController[0]/_Rect_CenterPanel[0]", OV_AUTO, 288.0f, 0.0f},
    {"c:FrontCanvas/Panel_Invitation_Panel(Clone)[9]/PanelActiveController[0]/_Rect_CenterPanel[0]/_Script_StoryProgress[1]/PanelActiveController[0]/Central_Panel[0]/_RectMask_Viewport[0]", OV_STRETCH, 0.0f, 0.0f},
    {"c:FrontCanvas/Panel_Invitation_Panel(Clone)[9]/PanelActiveController[0]/_Rect_CenterPanel[0]/_Script_StoryProgress[1]/PanelActiveController[0]", OV_STRETCH, 0.0f, 0.0f},
    {"c:FrontCanvas/Panel_BattlePagePanel(Clone)[12]/PanelActiveController[0]/_Image_PanelBlackout[0]", OV_STRETCH, 0.0f, 0.0f},
    {"c:FrontCanvas/Panel_EquipPageInvenPanel(Clone)[13]/PanelActiveController[0]/_Image_PanelBlackout[1]", OV_STRETCH, 0.0f, 0.0f},
    {"c:FrontCanvas/Librarian_Info_Panel(Clone)[14]/PanelActiveController[0]/_Rect_EquipCardSlotView[4]", OV_CENTER, 0.0f, 0.0f},
    {"c:FrontCanvas/Librarian_Info_Panel(Clone)[14]/PanelActiveController[0]/_Rect_EquipCardSlotView[4]/_Button_CustomSelectableGraphic[0]/_Image_buttonImage[0]", OV_CENTER, 0.0f, 0.0f},
    {"c:FrontCanvas/Librarian_Info_Panel(Clone)[14]/PanelActiveController[0]/_Rect_EquipCardSlotView[4]/_Button_CustomSelectableGraphic[0]", OV_CENTER, 0.0f, 0.0f},
    {"c:FrontCanvas/UIControlButton_Panel(Clone)[6]/PanelActiveController[0]/EnterTarget[0]/_Rect_MenuItems[3]/_Rect_LowerPivot[2]", OV_AUTO, 80.0f, 0.0f},
    {"c:FrontCanvas/UIControlButton_Panel(Clone)[6]/PanelActiveController[0]/EnterTarget[0]/_Rect_MenuItems[3]/_Button_MenuItem_Main[1]", OV_AUTO, 80.0f, 0.0f},
    {"c:FrontCanvas/UIControlButton_Panel(Clone)[6]/PanelActiveController[0]/EnterTarget[0]/_Rect_MenuItems[3]/Image[0]", OV_AUTO, 80.0f, 0.0f},
    {"c:FrontCanvas/UIControlButton_Panel(Clone)[6]/PanelActiveController[0]/EnterTarget[0]", OV_AUTO, 96.0f, 0.0f},
    {"c:FrontCanvas/LibrarianCharacterList_Panel(Clone)[7]/PanelActiveController[0]", OV_AUTO, -64.0f, 0.0f},
    {"c:FrontCanvas/_Rect_MainUIBgFrames[1]/_Rect_Right[1]/MainPanel_Bg_InnterImageRight[0]", OV_AUTO, -64.0f, 0.0f},
    {"c:FrontCanvas/Script_FloorInfo_Panel(Clone)[8]/PanelActiveController[0]/_Rect_Info_Panel[0]", OV_AUTO, -56.0f, 0.0f},
    {"c:FrontCanvas/Panel_BattlePagePanel(Clone)[12]/PanelActiveController[0]/_Script_Right_Panel[2]/_Script_LibrarianInfoCardPhase[0]", OV_AUTO, -40.0f, 0.0f},
    {"c:FrontCanvas/Panel_BattlePagePanel(Clone)[10]/PanelActiveController[0]/_Script_Right_Panel[2]", OV_AUTO, -60.0f, 0.0f},
    {"c:FrontCanvas/Panel_EquipPageInvenPanel(Clone)[11]/PanelActiveController[0]/_Script_Right_Panel[2]", OV_AUTO, -60.0f, 0.0f},
    {"c:FrontCanvas/Librarian_Info_Panel(Clone)[12]/PanelActiveController[0]/LibrarianInfo_AppearanceInfoPanel[5]", OV_AUTO, -60.0f, 0.0f},
    {"c:FrontCanvas/Panel_BattlePagePanel(Clone)[10]/PanelActiveController[0]/_Image_PanelBlackout[0]", OV_STRETCH, 0.0f, 0.0f},
    {"c:FrontCanvas/Panel_EquipPageInvenPanel(Clone)[11]/PanelActiveController[0]/_Image_PanelBlackout[1]", OV_STRETCH, 0.0f, 0.0f},
    {"c:FrontCanvas/_Rect_MainUIBgFrames[1]/_Rect_Left[0]/MainPanel_Bg_InnterImageLeft[0]", OV_AUTO, 112.0f, 0.0f},
    {"c:FrontCanvas/Panel_BattlePagePanel(Clone)[10]/PanelActiveController[0]/_Librarian_Left_Panel[3]", OV_AUTO, 112.0f, 0.0f},
    {"c:FrontCanvas/Panel_EquipPageInvenPanel(Clone)[11]/PanelActiveController[0]/_Rect_Left_Panel[3]", OV_AUTO, 96.0f, 0.0f},
    {"c:FrontCanvas/Panel_BattlePagePanel(Clone)[12]/PanelActiveController[0]/_Librarian_Left_Panel[3]", OV_AUTO, 128.0f, 0.0f},
    {"c:FrontCanvas/Panel_EquipPageInvenPanel(Clone)[13]/PanelActiveController[0]/_Rect_Left_Panel[3]/_Script_InvenLeftEquipPagePanel[0]", OV_AUTO, 112.0f, 0.0f},
    {"c:FrontCanvas/Panel_Invitation_Panel(Clone)[13]/PanelActiveController[0]/_Rect_CenterPanel[0]/_Script_StoryProgress[1]/PanelActiveController[0]", OV_STRETCH, 0.0f, 0.0f},
    {"c:FrontCanvas/Panel_Invitation_Panel(Clone)[13]/PanelActiveController[0]/_Rect_CenterPanel[0]/_Script_StoryProgress[1]", OV_STRETCH, 0.0f, 0.0f},
    {"c:FrontCanvas/Panel_Invitation_Panel(Clone)[13]/PanelActiveController[0]/_Rect_CenterPanel[0]/_Script_StoryProgress[1]/PanelActiveController[0]/Central_Panel[0]", OV_STRETCH, 0.0f, 0.0f},
};

static int ignore_builtin(void) {
    static int on = -1;
    if (on < 0) {
        const char *v = getenv("DM_MOD_FULLSCREEN_IGNORE_BUILTIN");
        on = v && v[0] == '1';
        LOG("[mod fullscreen-mode] built-in layout fixes %s (DM_MOD_FULLSCREEN_IGNORE_BUILTIN=%s)", on ? "ignored" : "used", v ? v : "unset");
    }
    return on;
}

static int ignore_layout(void) {
    static int on = -1;
    if (on < 0) {
        const char *v = getenv("DM_MOD_FULLSCREEN_IGNORE_LAYOUT");
        on = v && v[0] == '1';
        LOG("[mod fullscreen-mode] saved layout placements %s (DM_MOD_FULLSCREEN_IGNORE_LAYOUT=%s)", on ? "ignored" : "used", v ? v : "unset");
    }
    return on;
}

static int builtin_match(const char *key, int mode, float dx, float dy) {
    for (size_t i = 0; i < sizeof k_layout_default / sizeof *k_layout_default; i++) {
        const layout_default_t *d = &k_layout_default[i];
        char canon[K_KEY_MAX];
        key_canon(d->key, canon, sizeof canon);
        if (d->mode == mode && fabsf(d->dx - dx) < 0.05f && fabsf(d->dy - dy) < 0.05f && !strcmp(canon, key)) return 1;
    }
    return 0;
}

static int is_builtin_key(const char *key) {
    for (size_t i = 0; i < sizeof k_layout_default / sizeof *k_layout_default; i++) {
        char canon[K_KEY_MAX];
        key_canon(k_layout_default[i].key, canon, sizeof canon);
        if (!strcmp(canon, key)) return 1;
    }
    return 0;
}

static void layout_defaults(void) {
    if (ignore_builtin()) return;
    int n = 0;
    for (size_t i = 0; i < sizeof k_layout_default / sizeof *k_layout_default; i++) {
        const layout_default_t *d = &k_layout_default[i];
        ov_t *o = ov_make(d->key);
        if (!o) continue;
        o->mode = d->mode;
        o->dx = d->dx;
        o->dy = d->dy;
        n++;
    }
    LOG("[mod fullscreen-mode] layout: %d built-in placements loaded", n);
}

static int layout_load(void) {
    if (ignore_layout()) { LOG("[mod fullscreen-mode] layout: %s not read", layout_path()); return 0; }
    FILE *f = fopen(layout_path(), "r");
    if (!f) { LOG("[mod fullscreen-mode] layout: no file %s", layout_path()); return 0; }
    char line[K_KEY_MAX + 64];
    int n = 0;
    while (fgets(line, sizeof line, f)) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
        char *save = NULL, *key = strtok_r(line, "|", &save), *mode = strtok_r(NULL, "|", &save), *dx = strtok_r(NULL, "|", &save), *dy = strtok_r(NULL, "|\r\n", &save);
        int m = mode ? mode_parse(mode) : -1;
        if (!key || m < 0) continue;
        float fx = dx ? strtof(dx, NULL) : 0.0f, fy = dy ? strtof(dy, NULL) : 0.0f;
        ov_t *o = ov_make(key);
        if (!o) continue;
        o->mode = (uint8_t)m;
        o->dx = fx;
        o->dy = fy;
        n++;
    }
    fclose(f);
    LOG("[mod fullscreen-mode] layout: %d placements loaded from %s", n, layout_path());
    return 1;
}

static void layout_backup(void) {
    static int done;
    if (done || !ignore_layout()) return;
    done = 1;
    char bak[1024];
    snprintf(bak, sizeof bak, "%s.bak", layout_path());
    if (!rename(layout_path(), bak)) LOG("[mod fullscreen-mode] layout: the saved file was not loaded, so its old contents are kept as %s", bak);
}

static void layout_save(void) {
    layout_backup();
    FILE *f = fopen(layout_path(), "w");
    if (!f) { LOG("[mod fullscreen-mode] layout: cannot write %s", layout_path()); return; }
    fprintf(f, "# fullscreen-mode layout, one placement per line: path|mode|dx|dy (c: canvas paths in design units, w: scene paths in screen pixels, dy up; dx and dy are added to the position the automatic layout gives; auto|0|0 on a built-in placement turns it off)\n");
    int n = 0, builtin = !ignore_builtin();
    for (int i = 0; i < s_ov_n; i++) {
        const ov_t *o = &s_ov[i];
        if (!o->key[0] || (builtin && builtin_match(o->key, o->mode, o->dx, o->dy))) continue;
        fprintf(f, "%s|%s|%.1f|%.1f\n", o->key, k_ov_name[o->mode], o->dx, o->dy);
        n++;
    }
    fclose(f);
    LOG("[mod fullscreen-mode] layout: %d placements saved to %s", n, layout_path());
}

static bk_t *s_bk;
static uint32_t *s_bk_ix, s_bk_sig;
static int s_bk_n, s_bk_cap, s_bk_ixcap, s_bk_saved, s_bk_dirty, s_bk_rewrite, s_bk_save_at, s_bake = -1;

static const char *bake_path(void) {
    const char *v = getenv("DM_MOD_FULLSCREEN_BAKE_FILE");
    return v && v[0] ? v : K_BAKE_FILE;
}

static int bake_on(void) {
    if (s_bake < 0) {
        const char *v = getenv("DM_MOD_FULLSCREEN_BAKE");
        s_bake = v && v[0] == '1';
        if (s_bake) s_paths = 1;
        LOG("[mod fullscreen-mode] baked layout %s (DM_MOD_FULLSCREEN_BAKE=%s): element placements are remembered in %s and reused without running the layout algorithm", s_bake ? "on" : "off", v ? v : "unset", bake_path());
    }
    return s_bake == 1 && s_edit != 1;
}

static uint32_t bk_home(uint32_t h, uint32_t mask) {
    return ((h * 2654435761u) >> 7) & mask;
}

static void bk_ix_put(int idx) {
    uint32_t mask = (uint32_t)s_bk_ixcap - 1, i = bk_home(s_bk[idx].h, mask);
    while (s_bk_ix[i]) i = (i + 1) & mask;
    s_bk_ix[i] = (uint32_t)idx + 1;
}

static int bk_grow(void) {
    if (s_bk_cap >= K_BAKE_MAX) return 0;
    int cap = s_bk_cap ? s_bk_cap * 2 : 8192, ixcap = cap * 2;
    bk_t *p = realloc(s_bk, (size_t)cap * sizeof *p);
    if (!p) return 0;
    s_bk = p;
    uint32_t *ix = calloc((size_t)ixcap, sizeof *ix);
    if (!ix) return 0;
    free(s_bk_ix);
    s_bk_ix = ix;
    s_bk_ixcap = ixcap;
    s_bk_cap = cap;
    for (int i = 0; i < s_bk_n; i++) bk_ix_put(i);
    return 1;
}

static bk_t *bk_add(const bk_t *e) {
    if (s_bk_n >= s_bk_cap && !bk_grow()) return NULL;
    int idx = s_bk_n;
    s_bk[idx] = *e;
    bk_ix_put(idx);
    s_bk_n++;
    return &s_bk[idx];
}

static bk_t *bk_find(uint32_t h, double wc) {
    if (!s_bk_ix) return NULL;
    uint32_t mask = (uint32_t)s_bk_ixcap - 1, i = bk_home(h, mask);
    for (uint32_t k = 0; k < (uint32_t)s_bk_ixcap; k++) {
        uint32_t v = s_bk_ix[(i + k) & mask];
        if (!v) return NULL;
        bk_t *e = &s_bk[v - 1];
        if (e->h == h && fabs((double)e->wc - wc) < 0.5) return e;
    }
    return NULL;
}

static int bk_match(const bk_t *b, const rec_t *r, const par_t *pp) {
    return same4(r->oa, b->oa, 1e-5f) && same4(r->oo, b->oo, 1e-3f) && fabs(pp->act0 - b->pc[0]) < 0.5 && fabs(pp->asz - b->pc[1]) < 0.5 && fabs(pp->v0 - b->pc[2]) < 0.5 && fabs(pp->vsz - b->pc[3]) < 0.5 && fabs(pp->kp - b->pc[4]) < 0.001;
}

static void bake_record(bk_t *old, const rec_t *r, double wc, const par_t *pp, int act, int placed, int block, const float *fa, const float *fo) {
    bk_t e;
    memset(&e, 0, sizeof e);
    e.h = r->kh;
    e.wc = (float)wc;
    memcpy(e.oa, r->oa, sizeof e.oa);
    memcpy(e.oo, r->oo, sizeof e.oo);
    memcpy(e.fa, fa, sizeof e.fa);
    memcpy(e.fo, fo, sizeof e.fo);
    e.pc[0] = (float)pp->act0;
    e.pc[1] = (float)pp->asz;
    e.pc[2] = (float)pp->v0;
    e.pc[3] = (float)pp->vsz;
    e.pc[4] = (float)pp->kp;
    e.act = (uint8_t)act;
    e.placed = (uint8_t)placed;
    e.block = (uint8_t)block;
    if (old) {
        e.miss = (uint8_t)(old->miss + 1);
        *old = e;
        s_bk_rewrite = 1;
        s_bk_dirty = 1;
        return;
    }
    if (bk_add(&e)) s_bk_dirty = 1;
}

static void bk_pack(bk_t *e, float *g[5]) {
    g[0] = e->oa;
    g[1] = e->oo;
    g[2] = e->fa;
    g[3] = e->fo;
    g[4] = e->pc;
}

static void bk_write(FILE *f, const bk_t *e) {
    bk_t c = *e;
    float *g[5];
    static const int cnt[5] = {4, 4, 4, 4, 5};
    bk_pack(&c, g);
    fprintf(f, "%08x %.3f %d %d %d %d", c.h, c.wc, c.act, c.placed, c.block, c.miss);
    for (int i = 0; i < 5; i++)
        for (int k = 0; k < cnt[i]; k++) fprintf(f, " %.9g", g[i][k]);
    fputc('\n', f);
}

static int bk_parse(const char *s, bk_t *e) {
    const char *p = s;
    char *q = NULL;
    float *g[5];
    static const int cnt[5] = {4, 4, 4, 4, 5};
    long iv[4];
    memset(e, 0, sizeof *e);
    e->h = (uint32_t)strtoul(p, &q, 16);
    if (q == p) return 0;
    p = q;
    e->wc = strtof(p, &q);
    if (q == p) return 0;
    p = q;
    for (int i = 0; i < 4; i++) {
        iv[i] = strtol(p, &q, 10);
        if (q == p) return 0;
        p = q;
    }
    e->act = (uint8_t)iv[0];
    e->placed = (uint8_t)iv[1];
    e->block = (uint8_t)iv[2];
    e->miss = (uint8_t)iv[3];
    bk_pack(e, g);
    for (int i = 0; i < 5; i++)
        for (int k = 0; k < cnt[i]; k++) {
            g[i][k] = strtof(p, &q);
            if (q == p) return 0;
            p = q;
        }
    return e->h != 0 && e->act < ACT_COUNT;
}

static void bake_sig(void) {
    uint32_t sum = (uint32_t)K_BAKE_VER * 2654435761u;
    char buf[K_KEY_MAX + 48];
    for (int i = 0; i < s_ov_n; i++) {
        if (!s_ov[i].key[0]) continue;
        snprintf(buf, sizeof buf, "%s|%d|%.1f|%.1f", s_ov[i].key, s_ov[i].mode, s_ov[i].dx, s_ov[i].dy);
        sum += fnv(2166136261u, buf);
    }
    s_bk_sig = sum;
}

static void bake_load(void) {
    FILE *f = fopen(bake_path(), "r");
    if (!f) { s_bk_rewrite = 1; LOG("[mod fullscreen-mode] baked layout: no file %s, placements are recorded as the UI is seen", bake_path()); return; }
    char line[512];
    int ver = 0, n = 0;
    unsigned sig = 0;
    if (!fgets(line, sizeof line, f) || sscanf(line, "# fullscreen-mode baked layout v%d sig %x", &ver, &sig) != 2 || ver != K_BAKE_VER || sig != s_bk_sig) {
        fclose(f);
        s_bk_rewrite = 1;
        LOG("[mod fullscreen-mode] baked layout: %s is for another layout or version, starting over", bake_path());
        return;
    }
    while (fgets(line, sizeof line, f)) {
        bk_t e;
        if (line[0] == '#' || !bk_parse(line, &e)) continue;
        if (bk_add(&e)) n++;
    }
    fclose(f);
    s_bk_saved = s_bk_n;
    LOG("[mod fullscreen-mode] baked layout: %d placements loaded from %s", n, bake_path());
}

static void bake_save(void) {
    if (!s_bk_dirty) return;
    FILE *f = fopen(bake_path(), s_bk_rewrite ? "w" : "a");
    if (!f) { LOG("[mod fullscreen-mode] baked layout: cannot write %s", bake_path()); s_bk_dirty = 0; return; }
    int from = s_bk_saved;
    if (s_bk_rewrite) {
        fprintf(f, "# fullscreen-mode baked layout v%d sig %08x\n", K_BAKE_VER, s_bk_sig);
        from = 0;
    }
    for (int i = from; i < s_bk_n; i++) bk_write(f, &s_bk[i]);
    fclose(f);
    LOG("[mod fullscreen-mode] baked layout: %d placements %s %s", s_bk_n - from, s_bk_rewrite ? "written to" : "appended to", bake_path());
    s_bk_saved = s_bk_n;
    s_bk_rewrite = 0;
    s_bk_dirty = 0;
}

static void bake_open(void) {
    if (!bake_on()) return;
    bake_sig();
    bake_load();
}

static void bake_flush(void) {
    if (!bake_on() || !s_bk_dirty || g_frame < s_bk_save_at) return;
    s_bk_save_at = g_frame + K_BAKE_SAVE_EVERY;
    bake_save();
}

static int ui_every(void) {
    static int v;
    if (!v) {
        const char *e = getenv("DM_MOD_FULLSCREEN_UI_EVERY");
        int n = e ? atoi(e) : 0;
        v = n > 0 ? n : (bake_on() ? K_UI_EVERY_BAKED : K_UI_EVERY);
    }
    return v;
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
    np->grp_ok = 1;
    np->n = 0;
    np->ptf = NULL;
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
static int group_centred(void *tf, const cv_t *cv, const par_t *pp, int n);

static int grp_resolve(const cv_t *cv, par_t *pp) {
    if (!pp->grp_ok) {
        pp->grp_ok = 1;
        pp->grp = pp->through || !pp->ptf ? 0 : group_centred(pp->ptf, cv, pp, pp->n);
    }
    return pp->grp;
}

static void process_node(void *tf, const cv_t *cv, par_t *pp, int depth) {
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
    r->calc = 0;
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
            calc_set(r, 2, sact, sh, sh, 0.0, 0.0);
            if (place_axis(tf, ax, pp->act0, pp->asz, pp->kp, d0 + sh, d1 + sh, &a0s, &a1s)) {
                rt_read(tf, ca, co);
                memcpy(r->aa, ca, sizeof r->aa);
                memcpy(r->ao, co, sizeof r->ao);
                r->keep = 1;
                g_changed++;
            }
            g_act[sact]++;
        } else {
            calc_set(r, 2, ACT_SCALED, 0.0, 0.0, 0.0, 0.0);
            g_act[ACT_SCALED]++;
        }
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
    int span = d0 <= K_FULL_TOL && cv->design - d1 <= K_FULL_TOL;
    int bake = bake_on() && r->kh && !span;
    bk_t *bk = bake ? bk_find(r->kh, cv->size) : NULL;
    if (bk && bk->miss >= K_BAKE_MISS_MAX) bake = 0;
    if (bake && bk && bk_match(bk, r, pp)) {
        int bact = bk->act, bblock = bk->block;
        g_act[bact]++;
        if (bact == ACT_SKIP) return;
        if (bk->placed) {
            if (!same4(ca, bk->fa, 1e-5f) || !same4(co, bk->fo, 1e-3f)) {
                rt_write_anchors(tf, bk->fa);
                rt_write_offsets(tf, bk->fo);
                rt_read(tf, ca, co);
                memcpy(r->aa, ca, sizeof r->aa);
                memcpy(r->ao, co, sizeof r->ao);
                g_changed++;
            }
            r->keep = 1;
        }
        double ba0 = pp->act0 + (ca[ax] * pp->asz + co[ax]) * pp->kp;
        if (depth >= K_MAX_DEPTH || (bblock && !anc)) return;
        par_t bp;
        child_par(&bp, cv, pp, r, ca, co, d0, lo, hi, ba0, v0, kpn);
        bp.through = bblock;
        process_children(tf, cv, &bp, depth + 1);
        return;
    }
    int nchild = -1, art = span && node_art(tf, r);
    double t0 = 0.0, t1 = 0.0;
    int act = classify(d0, d1, cv, art, &t0, &t1);
    if ((act == ACT_LEFT || act == ACT_RIGHT) && grp_resolve(cv, pp)) {
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
    int moves = act != ACT_SKIP && act != ACT_THRU;
    calc_set(r, moves ? 1 : 2, act, moves ? t0 - d0 : 0.0, moves ? t1 - d1 : 0.0, 0.0, 0.0);
    if (ov && moves) { t0 += ov->dx; t1 += ov->dx; }
    g_act[act]++;
    double a0 = pp->act0 + (ca[ax] * pp->asz + co[ax]) * pp->kp;
    double a1 = pp->act0 + (ca[2 + ax] * pp->asz + co[2 + ax]) * pp->kp;
    if (act == ACT_SKIP) {
        if (bake) bake_record(bk, r, cv->size, pp, act, 0, 1, ca, co);
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
            double y0 = mid + (v0 - mid) * f, y1 = mid + (v1 - mid) * f;
            r->base[2] = (float)(y0 - v0);
            r->base[3] = (float)(y1 - v1);
            changed |= place_axis(tf, ay, pp->v0, pp->vsz, pp->kp, y0 + dy, y1 + dy, &b0, &b1);
        } else if (ov || r->cover) {
            changed |= place_axis(tf, ay, pp->v0, pp->vsz, pp->kp, v0 + dy, v1 + dy, &b0, &b1);
        }
        r->cover = act == ACT_COVER;
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
    if (bake) bake_record(bk, r, cv->size, pp, act, act != ACT_THRU, block, ca, co);
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
    int n = child_count(tf);
    par_t lp = *pp0;
    lp.ptf = tf;
    lp.n = n;
    lp.grp = 0;
    lp.grp_ok = pp0->through;
    par_t *pp = &lp;
    for (int i = 0; i < n && g_budget > 0; i++) {
        void *child = child_at(tf, i);
        if (!child || child == s_hl_tf || (g_skip_inactive && !active_node(child))) continue;
        s_idx = i;
        process_node(child, cv, pp, depth);
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
    g_next_ui = g_frame + ui_every();
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
    bake_flush();
    static int starved_logged;
    if (g_starved && starved_logged++ < 20) LOG("[mod fullscreen-mode] frame %d: node budget %d used up at canvas %d of %d, the next sweep resumes there", g_frame, K_NODE_BUDGET, starved_at, (int)n);
    static int logged;
    if ((matched || g_changed) && logged++ < 40)
        LOG("[mod fullscreen-mode] frame %d: %d of %d canvas scalers set to match, %d canvases laid out (screen %dx%d): %d elements adjusted of %d seen (%d left, %d right, %d centred, %d stretched, %d expanded, %d containers passed, %d left alone)", g_frame, matched, scalers, laid, sw, sh, g_changed, g_act[ACT_LEFT] + g_act[ACT_RIGHT] + g_act[ACT_CENTER] + g_act[ACT_STRETCH] + g_act[ACT_COVER] + g_act[ACT_THRU] + g_act[ACT_SKIP] + g_act[ACT_SCALED], g_act[ACT_LEFT], g_act[ACT_RIGHT], g_act[ACT_CENTER], g_act[ACT_STRETCH], g_act[ACT_COVER], g_act[ACT_THRU], g_act[ACT_SKIP] + g_act[ACT_SCALED]);
}

__attribute__((weak)) void front_layout_panel(int show) { (void)show; }
__attribute__((weak)) void front_layout_state(const char *key, int mode, int index, int total, int placements, const char *how, const char *calc) { (void)key; (void)mode; (void)index; (void)total; (void)placements; (void)how; (void)calc; }

static void q_push(int kind, float a, float b) {
    pthread_mutex_lock(&s_qm);
    int next = (s_qt + 1) % 64;
    if (next != s_qh) {
        s_q[s_qt] = (fs_cmd_t){kind, a, b};
        s_qt = next;
    }
    pthread_mutex_unlock(&s_qm);
}

static int q_pop(fs_cmd_t *c) {
    pthread_mutex_lock(&s_qm);
    int ok = s_qh != s_qt;
    if (ok) {
        *c = s_q[s_qh];
        s_qh = (s_qh + 1) % 64;
    }
    pthread_mutex_unlock(&s_qm);
    return ok;
}

void dm_layout_pick(float nx, float ny) { q_push(0, nx, ny); }

void dm_layout_cmd(int cmd, float a, float b) { q_push(cmd, a, b); }

static int edit_enabled(void) {
    if (s_edit < 0) {
        const char *v = getenv("DM_MOD_FULLSCREEN_EDIT");
        s_edit = v && v[0] == '1';
        if (s_edit) s_paths = 1;
        LOG("[mod fullscreen-mode] layout editor %s (DM_MOD_FULLSCREEN_EDIT=%s): the Layout button opens the editor window, with Select on a tap picks an element (a UI element or a scene object), the window sets its placement and nudges it, Save writes %s", s_edit ? "on" : "off", v ? v : "unset", layout_path());
    }
    return s_edit;
}

static pick_t *pick_slot(void) {
    if (s_pick_n >= s_pick_cap) {
        int cap = s_pick_cap ? s_pick_cap * 2 : 128;
        pick_t *p = realloc(s_pick, (size_t)cap * sizeof *p);
        if (!p) return NULL;
        s_pick = p;
        s_pick_cap = cap;
    }
    return &s_pick[s_pick_n];
}

static int is_world_key(const char *key) {
    return key[0] == 'w' && key[1] == ':';
}

static void *rend_type(void) {
    if (!g_rend_type && g_rend_tries < K_TYPE_TRIES) {
        g_rend_tries++;
        g_rend_type = find_type("UnityEngine.Renderer, UnityEngine.CoreModule");
        LOG("[mod fullscreen-mode] types: Renderer %p", g_rend_type);
    }
    return g_rend_type;
}

static int near3(vec3 a, vec3 b) {
    return fabsf(a.x - b.x) < K_WORLD_EPS && fabsf(a.y - b.y) < K_WORLD_EPS && fabsf(a.z - b.z) < K_WORLD_EPS;
}

static vec3 tf_position(void *tf) {
    return FN(K_TRANSFORM_GET_POSITION, vec3, void *, const void *)(tf, NULL);
}

static vec3 tf_scale(void *tf) {
    return FN(K_TRANSFORM_GET_LOCALSCALE, vec3, void *, const void *)(tf, NULL);
}

static int world_key(void *tf, char *out, size_t cap, char *leaf) {
    char seg[K_PICK_DEPTH + 2][K_SEG_MAX];
    int n = 0;
    void *cur = tf;
    while (cur && alive(cur) && n < K_PICK_DEPTH) {
        int idx = FN(K_TRANSFORM_GET_SIBLING, int, void *, const void *)(cur, NULL);
        seg_make(cur, idx, seg[n++]);
        cur = FN(K_TRANSFORM_GET_PARENT, void *, void *, const void *)(cur, NULL);
    }
    if (n == 0 || (cur && alive(cur))) return 0;
    size_t len = 0;
    int w = snprintf(out, cap, "w:");
    if (w < 0 || (size_t)w >= cap) return 0;
    len = (size_t)w;
    for (int i = n - 1; i >= 0; i--) {
        w = snprintf(out + len, cap - len, i == n - 1 ? "%s" : "/%s", seg[i]);
        if (w < 0 || len + (size_t)w >= cap) return 0;
        len += (size_t)w;
    }
    if (leaf) snprintf(leaf, K_SEG_MAX, "%s", seg[0]);
    return 1;
}

static void *world_camera(void *go) {
    void *arr = FN(K_CAMERA_GET_ALL, void *, const void *)(NULL);
    uint64_t n = arr ? *(const uint64_t *)((uintptr_t)arr + F_ARRAY_LEN) : 0;
    int layer = FN(K_GO_GET_LAYER, int, void *, const void *)(go, NULL);
    void *best = NULL;
    float best_depth = 0.0f;
    for (uint64_t i = 0; i < n; i++) {
        void *cam = *(void *const *)((uintptr_t)arr + F_ARRAY_DATA + 8 * i);
        if (!alive(cam)) continue;
        if (!((FN(K_CAMERA_GET_MASK, int, void *, const void *)(cam, NULL) >> layer) & 1)) continue;
        if (alive(FN(K_CAMERA_GET_TEXTURE, void *, void *, const void *)(cam, NULL))) continue;
        float depth = FN(K_CAMERA_GET_DEPTH, float, void *, const void *)(cam, NULL);
        if (!best || depth >= best_depth) { best = cam; best_depth = depth; }
    }
    return best;
}

static int w2s(void *cam, vec3 p, double *x, double *y) {
    vec3 r = FN(K_CAMERA_W2S, vec3, void *, vec3, const void *)(cam, p, NULL);
    if (r.z <= 0.0f && !(FN(K_CAMERA_GET_ORTHO, uint8_t, void *, const void *)(cam, NULL) & 1)) return 0;
    *x = r.x;
    *y = r.y;
    return 1;
}

static int bounds_rect(void *cam, const bounds_t *b, double *x0, double *x1, double *y0, double *y1) {
    double lx = 0.0, hx = 0.0, ly = 0.0, hy = 0.0;
    int got = 0;
    for (int k = 0; k < 8; k++) {
        vec3 p = {b->x + ((k & 1) ? b->ex : -b->ex), b->y + ((k & 2) ? b->ey : -b->ey), b->z + ((k & 4) ? b->ez : -b->ez)};
        double x, y;
        if (!w2s(cam, p, &x, &y)) continue;
        if (!got || x < lx) lx = x;
        if (!got || x > hx) hx = x;
        if (!got || y < ly) ly = y;
        if (!got || y > hy) hy = y;
        got = 1;
    }
    *x0 = lx;
    *x1 = hx;
    *y0 = ly;
    *y1 = hy;
    return got && hx - lx > 1.0 && hy - ly > 1.0;
}

static int renderer_rect(void *cam, void *rend, double *x0, double *x1, double *y0, double *y1) {
    bounds_t b = FN(K_RENDERER_GET_BOUNDS, bounds_t, void *, const void *)(rend, NULL);
    return bounds_rect(cam, &b, x0, x1, y0, y1);
}

static void world_restore(wld_t *w) {
    if (w->have && w->tf && alive(w->tf)) {
        FN(K_TRANSFORM_SET_LOCALSCALE, void, void *, vec3, const void *)(w->tf, w->bs, NULL);
        FN(K_TRANSFORM_SET_POSITION, void, void *, vec3, const void *)(w->tf, w->bp, NULL);
    }
}

static void world_release(int idx) {
    if (idx < 0 || idx >= K_OV_MAX) return;
    world_restore(&s_wld[idx]);
    memset(&s_wld[idx], 0, sizeof s_wld[idx]);
}

static void world_resolve(void) {
    void *rt = rend_type();
    if (!rt) return;
    char leaf[K_SEG_MAX];
    int want = 0;
    for (int i = 0; i < s_ov_n; i++)
        if (s_ov[i].key[0] && is_world_key(s_ov[i].key) && (!s_wld[i].tf || !alive(s_wld[i].tf))) want++;
    if (!want) return;
    void *arr = FN(K_RESOURCES_FINDALL, void *, void *, const void *)(rt, NULL);
    uint64_t n = arr ? *(const uint64_t *)((uintptr_t)arr + F_ARRAY_LEN) : 0;
    for (uint64_t k = 0; k < n && want > 0; k++) {
        void *r = *(void *const *)((uintptr_t)arr + F_ARRAY_DATA + 8 * k);
        if (!alive(r)) continue;
        void *tf = FN(K_COMPONENT_GET_TRANSFORM, void *, void *, const void *)(r, NULL);
        if (!alive(tf) || *(void *const *)tf == s_rt_klass || !active_node(tf)) continue;
        seg_make(tf, FN(K_TRANSFORM_GET_SIBLING, int, void *, const void *)(tf, NULL), leaf);
        for (int i = 0; i < s_ov_n; i++) {
            ov_t *o = &s_ov[i];
            if (!o->key[0] || !is_world_key(o->key) || (s_wld[i].tf && alive(s_wld[i].tf))) continue;
            const char *last = strrchr(o->key, '/');
            if (strcmp(last ? last + 1 : o->key + 2, leaf)) continue;
            char key[K_KEY_MAX];
            if (!world_key(tf, key, sizeof key, NULL) || key_hash(key, 0) != o->h) continue;
            memset(&s_wld[i], 0, sizeof s_wld[i]);
            s_wld[i].tf = tf;
            s_wld[i].rend = r;
            want--;
            LOG("[mod fullscreen-mode] world: \"%s\" found", o->key);
        }
    }
}

static void world_apply(int i) {
    ov_t *o = &s_ov[i];
    wld_t *w = &s_wld[i];
    w->calc = 0;
    void *go = FN(K_COMPONENT_GET_GAMEOBJECT, void *, void *, const void *)(w->tf, NULL);
    if (!alive(go) || !alive(w->rend)) return;
    if (!w->cam || !alive(w->cam)) w->cam = world_camera(go);
    if (!w->cam) return;
    int pw = FN(K_CAMERA_GET_PW, int, void *, const void *)(w->cam, NULL), ph = FN(K_CAMERA_GET_PH, int, void *, const void *)(w->cam, NULL);
    if (pw <= 0 || ph <= 0) return;
    if (w->have && (pw != w->pw || ph != w->ph)) {
        world_restore(w);
        w->have = 0;
    }
    vec3 cp = tf_position(w->tf), cs = tf_scale(w->tf);
    if (w->have && (!near3(cp, w->ap) || !near3(cs, w->as))) w->have = 0;
    if (!w->have) {
        w->bp = cp;
        w->bs = cs;
        w->bb = FN(K_RENDERER_GET_BOUNDS, bounds_t, void *, const void *)(w->rend, NULL);
        w->pw = pw;
        w->ph = ph;
        w->have = 1;
        w->ap = cp;
        w->as = cs;
    }
    double bx0, bx1, by0, by1, px, py, ex, ey, fx = 1.0, fy = 1.0;
    if (!bounds_rect(w->cam, &w->bb, &bx0, &bx1, &by0, &by1) || !w2s(w->cam, w->bp, &px, &py)) return;
    vec3 ox = {w->bp.x + 1.0f, w->bp.y, w->bp.z}, oy = {w->bp.x, w->bp.y + 1.0f, w->bp.z};
    double jx0, jx1, jy0, jy1;
    if (!w2s(w->cam, ox, &jx0, &jx1) || !w2s(w->cam, oy, &jy0, &jy1)) return;
    double jxx = jx0 - px, jxy = jx1 - py, jyx = jy0 - px, jyy = jy1 - py;
    double det = jxx * jyy - jyx * jxy;
    if (fabs(det) < 1e-9) return;
    double dwp = ph * K_DESIGN_ASPECT, pad = pw > dwp ? (pw - dwp) / 2.0 : 0.0;
    cv_t cv = {0, (double)pw, dwp, (double)ph, 1.0};
    double t0 = bx0, t1 = bx1, ty0 = by0, ty1 = by1, bw = bx1 - bx0;
    if (o->mode != OV_AUTO) ov_apply(o->mode, bx0 - pad, bx1 - pad, &cv, &t0, &t1);
    if (o->mode == OV_STRETCH && bw > 1e-3) fx = (t1 - t0) / bw;
    if (o->mode == OV_EXPAND && dwp > 1.0) {
        double f = (double)pw / dwp, mid = ph / 2.0;
        fx = fy = f;
        ty0 = mid + (by0 - mid) * f;
        ty1 = mid + (by1 - mid) * f;
    }
    fx = fmin(fmax(fx, 0.05), 20.0);
    fy = fmin(fmax(fy, 0.05), 20.0);
    w->calc = 1;
    w->base[0] = t0 - bx0;
    w->base[1] = t1 - bx1;
    w->base[2] = ty0 - by0;
    w->base[3] = ty1 - by1;
    t0 += o->dx;
    ty0 += o->dy;
    ex = px + (bx0 - px) * fx;
    ey = py + (by0 - py) * fy;
    double dpx = t0 - ex, dpy = ty0 - ey;
    double wx = (dpx * jyy - jyx * dpy) / det, wy = (jxx * dpy - jxy * dpx) / det;
    vec3 np = {w->bp.x + (float)wx, w->bp.y + (float)wy, w->bp.z};
    vec3 ns = {w->bs.x * (float)fx, w->bs.y * (float)fy, w->bs.z};
    if (!near3(ns, cs)) FN(K_TRANSFORM_SET_LOCALSCALE, void, void *, vec3, const void *)(w->tf, ns, NULL);
    if (!near3(np, cp)) FN(K_TRANSFORM_SET_POSITION, void, void *, vec3, const void *)(w->tf, np, NULL);
    w->ap = tf_position(w->tf);
    w->as = tf_scale(w->tf);
}

static void world_tick(void) {
    if (g_state != 1 || !g_applied) return;
    int any = 0, lost = 0;
    for (int i = 0; i < s_ov_n; i++) {
        if (!s_ov[i].key[0] || !is_world_key(s_ov[i].key)) continue;
        any = 1;
        if (!s_wld[i].tf || !alive(s_wld[i].tf)) {
            memset(&s_wld[i], 0, sizeof s_wld[i]);
            lost = 1;
        }
    }
    if (!any) return;
    if (lost && g_frame >= s_wld_next) {
        s_wld_next = g_frame + K_WORLD_RETRY;
        world_resolve();
    }
    for (int i = 0; i < s_ov_n; i++)
        if (s_ov[i].key[0] && is_world_key(s_ov[i].key) && s_wld[i].tf) world_apply(i);
}

static void pick_world(float nx, float ny) {
    void *rt = rend_type();
    if (!rt) return;
    void *arr = FN(K_RESOURCES_FINDALL, void *, void *, const void *)(rt, NULL);
    uint64_t n = arr ? *(const uint64_t *)((uintptr_t)arr + F_ARRAY_LEN) : 0;
    int first = s_pick_n;
    for (uint64_t k = 0; k < n; k++) {
        void *r = *(void *const *)((uintptr_t)arr + F_ARRAY_DATA + 8 * k);
        if (!alive(r) || !(FN(K_RENDERER_GET_ENABLED, uint8_t, void *, const void *)(r, NULL) & 1)) continue;
        void *tf = FN(K_COMPONENT_GET_TRANSFORM, void *, void *, const void *)(r, NULL);
        if (!alive(tf) || *(void *const *)tf == s_rt_klass || !active_node(tf)) continue;
        int dup = 0;
        for (int d = first; d < s_pick_n && !dup; d++) dup = s_pick[d].tf == tf;
        if (dup) continue;
        void *go = FN(K_COMPONENT_GET_GAMEOBJECT, void *, void *, const void *)(tf, NULL);
        void *cam = alive(go) ? world_camera(go) : NULL;
        if (!cam) continue;
        int pw = FN(K_CAMERA_GET_PW, int, void *, const void *)(cam, NULL), ph = FN(K_CAMERA_GET_PH, int, void *, const void *)(cam, NULL);
        if (pw <= 0 || ph <= 0) continue;
        double x0, x1, y0, y1, tx = (double)nx * pw, ty = (1.0 - (double)ny) * ph;
        if (!renderer_rect(cam, r, &x0, &x1, &y0, &y1) || tx < x0 || tx > x1 || ty < y0 || ty > y1) continue;
        pick_t *slot = pick_slot();
        if (!slot) return;
        if (!world_key(tf, slot->key, sizeof slot->key, NULL)) continue;
        slot->tf = tf;
        slot->rend = r;
        slot->cam = cam;
        slot->h = key_hash(slot->key, 0);
        slot->depth = 0;
        slot->world = 1;
        slot->area = (x1 - x0) * (y1 - y0) * s_unit * s_unit;
        s_pick_n++;
    }
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
        pick_t *k = px >= x0 && px <= x1 && py >= y0 && py <= y1 && x1 - x0 > 1.0 && y1 - y0 > 1.0 ? pick_slot() : NULL;
        if (k) {
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
                k->rend = NULL;
                k->cam = NULL;
                k->h = h;
                k->depth = depth;
                k->world = 0;
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
    g_budget = K_PICK_BUDGET;
    s_unit = 1.0;
    int unit_set = 0;
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
        s_ui_root = tf;
        if (!unit_set) { s_unit = hc / sh; unit_set = 1; }
        pk_t root = {0.0, wc, 0.0, hc, 1.0};
        pick_walk(tf, &cv, &root, 0, fnv(2166136261u, s_pseg[0]), (double)nx * wc, (1.0 - (double)ny) * hc);
    }
    pick_world(nx, ny);
    if (s_pick_n > 1) qsort(s_pick, (size_t)s_pick_n, sizeof s_pick[0], pick_cmp);
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

static void hl_make(void *parent, const float *anchors) {
    hl_clear();
    if (!parent || !alive(parent)) return;
    void *src = hl_source();
    if (!src) { LOG("[mod fullscreen-mode] edit: no image to build the highlight from, the selection is only logged"); return; }
    void *img = FN(K_OBJECT_INSTANTIATE_PARENT, void *, void *, void *, const void *)(src, parent, NULL);
    if (!alive(img)) return;
    void *tf = FN(K_COMPONENT_GET_TRANSFORM, void *, void *, const void *)(img, NULL);
    if (!alive(tf)) return;
    float a[4] = {0.0f, 0.0f, 1.0f, 1.0f}, o[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    if (anchors) memcpy(a, anchors, sizeof a);
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

static float s_hl_a[4];

static void hl_world(void) {
    double x0, x1, y0, y1;
    if (!s_ui_root || !alive(s_ui_root) || !s_sel_rend || !alive(s_sel_rend) || !s_sel_cam || !alive(s_sel_cam)) { hl_clear(); return; }
    int pw = FN(K_CAMERA_GET_PW, int, void *, const void *)(s_sel_cam, NULL), ph = FN(K_CAMERA_GET_PH, int, void *, const void *)(s_sel_cam, NULL);
    if (pw <= 0 || ph <= 0 || !renderer_rect(s_sel_cam, s_sel_rend, &x0, &x1, &y0, &y1)) { hl_clear(); return; }
    float a[4] = {(float)(x0 / pw), (float)(y0 / ph), (float)(x1 / pw), (float)(y1 / ph)};
    if (s_hl_tf && alive(s_hl_tf)) {
        if (!same4(a, s_hl_a, 1e-5f)) rt_write_anchors(s_hl_tf, a);
    } else hl_make(s_ui_root, a);
    memcpy(s_hl_a, a, sizeof s_hl_a);
}

static void edit_follow(void) {
    if (!edit_enabled() || g_state != 1 || !s_sel_world || !s_sel_tf || !alive(s_sel_tf)) return;
    hl_world();
}

static void sel_set(const pick_t *k) {
    s_sel_tf = k->tf;
    s_sel_rend = k->rend;
    s_sel_cam = k->cam;
    s_sel_world = k->world;
    s_sel_h = k->h;
    snprintf(s_sel_key, sizeof s_sel_key, "%s", k->key);
    if (k->world) hl_world();
    else hl_make(s_sel_tf, NULL);
    const ov_t *o = ov_find(s_sel_h);
    LOG("[mod fullscreen-mode] edit: selected %d of %d \"%s\": %s, dx %.0f, dy %.0f", s_pick_i + 1, s_pick_n, s_sel_key, k_ov_name[o ? o->mode : OV_AUTO], o ? o->dx : 0.0f, o ? o->dy : 0.0f);
}

static void sel_clear(void) {
    hl_clear();
    s_sel_tf = NULL;
    s_sel_rend = s_sel_cam = NULL;
    s_sel_world = 0;
    s_sel_h = 0;
    s_sel_key[0] = 0;
    s_pick_n = 0;
}

static int ov_live(void) {
    int n = 0;
    for (int i = 0; i < s_ov_n; i++)
        if (s_ov[i].key[0]) n++;
    return n;
}

static char s_how[128], s_calc[256];

static void num_text(char *out, size_t cap, double v) {
    if (fabs(v) < 0.5) snprintf(out, cap, "0");
    else snprintf(out, cap, "%+.0f", v);
}

static void span_text(char *out, size_t cap, double a, double b) {
    char x[24], y[24];
    num_text(x, sizeof x, a);
    if (fabs(a - b) < 0.5) { snprintf(out, cap, "%s", x); return; }
    num_text(y, sizeof y, b);
    snprintf(out, cap, "%s..%s", x, y);
}

static void calc_text(char *how, size_t hcap, char *txt, size_t tcap) {
    const ov_t *o = ov_find(s_sel_h);
    double ux = o ? o->dx : 0.0, uy = o ? o->dy : 0.0, b[4] = {0.0, 0.0, 0.0, 0.0};
    int state = 1;
    const char *name = "not moved by the layout";
    if (s_sel_world) {
        const wld_t *w = o ? &s_wld[o - s_ov] : NULL;
        if (!o) name = "auto, left alone";
        else if (w->calc) {
            name = k_ov_name[o->mode];
            memcpy(b, w->base, sizeof b);
        } else name = "object not found yet";
    } else {
        const rec_t *r = rec_find(s_sel_tf);
        if (r && r->calc) {
            state = r->calc;
            name = k_act_name[r->cact];
            for (int i = 0; i < 4; i++) b[i] = r->base[i];
        }
    }
    snprintf(how, hcap, "%s (%s)", name, s_sel_world ? "screen pixels" : "design units");
    char ax[48], ay[48], tx[48], ty[48], mx[24], my[24];
    span_text(ax, sizeof ax, b[0], b[1]);
    span_text(ay, sizeof ay, b[2], b[3]);
    if (state == 2) {
        snprintf(txt, tcap, "algorithm  x %s   y %s\nyours      no effect here\ntotal      x %s   y %s", ax, ay, ax, ay);
        return;
    }
    num_text(mx, sizeof mx, ux);
    num_text(my, sizeof my, uy);
    span_text(tx, sizeof tx, b[0] + ux, b[1] + ux);
    span_text(ty, sizeof ty, b[2] + uy, b[3] + uy);
    snprintf(txt, tcap, "algorithm  x %s   y %s\nyours      x %s   y %s\ntotal      x %s   y %s", ax, ay, mx, my, tx, ty);
}

static void push_state(void) {
    const ov_t *o = ov_find(s_sel_h);
    if (s_sel_tf) calc_text(s_how, sizeof s_how, s_calc, sizeof s_calc);
    else s_how[0] = s_calc[0] = 0;
    front_layout_state(s_sel_tf ? s_sel_key : "", o ? o->mode : OV_AUTO, s_pick_i + 1, s_pick_n, ov_live(), s_how, s_calc);
}

static void edit_refresh(void) {
    if (!edit_enabled() || g_state != 1 || !s_sel_tf || !alive(s_sel_tf)) return;
    char how[sizeof s_how], txt[sizeof s_calc];
    calc_text(how, sizeof how, txt, sizeof txt);
    if (strcmp(how, s_how) || strcmp(txt, s_calc)) push_state();
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
    if (o && o->mode == OV_AUTO && o->dx == 0.0f && o->dy == 0.0f && (ignore_builtin() || !is_builtin_key(o->key))) {
        ov_drop(o);
        if (s_hl_img && alive(s_hl_img)) FN(K_GRAPHIC_SET_COLOR, void, void *, rgba, const void *)(s_hl_img, k_ov_color[OV_AUTO], NULL);
        LOG("[mod fullscreen-mode] edit: \"%s\" back to auto", s_sel_key);
        g_next_ui = g_frame;
    } else edit_changed(o);
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

static void edit_exec(const fs_cmd_t *c) {
    if (c->kind == 0) { edit_tap(c->a, c->b); return; }
    if (c->kind == DM_LAYOUT_SAVE) { layout_save(); return; }
    if (c->kind == DM_LAYOUT_DESELECT) { sel_clear(); return; }
    if (!s_sel_tf) return;
    if (c->kind == DM_LAYOUT_STEP) {
        if (s_pick_n > 0) {
            s_pick_i = (s_pick_i + s_pick_n + (c->a < 0.0f ? -1 : 1)) % s_pick_n;
            sel_set(&s_pick[s_pick_i]);
        }
        return;
    }
    if (c->kind == DM_LAYOUT_MODE) {
        int m = (int)c->a;
        if (m < 0 || m >= OV_COUNT) return;
        ov_t *o = sel_ov();
        if (!o) return;
        o->mode = (uint8_t)m;
        sel_tidy(o);
        return;
    }
    if (c->kind == DM_LAYOUT_NUDGE) {
        ov_t *o = sel_ov();
        if (!o) return;
        o->dx += c->a;
        o->dy += c->b;
        sel_tidy(o);
        return;
    }
    if (c->kind == DM_LAYOUT_RESET) {
        ov_t *o = ov_find(s_sel_h);
        if (o) {
            o->mode = OV_AUTO;
            o->dx = o->dy = 0.0f;
            sel_tidy(o);
        }
    }
}

static void edit_run(void) {
    if (!edit_enabled() || g_state != 1 || !g_applied) return;
    static int shown;
    if (!shown) { shown = 1; front_layout_panel(1); push_state(); }
    if (s_hl_img && !alive(s_hl_img)) hl_clear();
    if (s_sel_tf && !alive(s_sel_tf)) sel_clear();
    fs_cmd_t c;
    int any = 0;
    while (q_pop(&c)) { edit_exec(&c); any = 1; }
    if (any) push_state();
}

static void frame_run(uint64_t *r, int param) {
    (void)r; (void)param;
    if (g_state < 0 || !main_base()) return;
    if (g_state == 0) {
        if (strncmp(dm_main_build(), K_BUILD_ID, 32) != 0) { g_state = -1; LOG("[mod fullscreen-mode] wrong build, mod stays out"); return; }
        g_state = 1;
        edit_enabled();
        if (!s_loaded) { s_loaded = 1; layout_defaults(); layout_load(); bake_open(); }
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
    world_tick();
    edit_follow();
    ui_run(r, param);
    edit_refresh();
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
