// Based on Bat'ko Makhno's Resolution Fix (Steam Workshop 2893583716)
#include "dm.h"
#include "hle_mods_gen.h"

void dm_mod_gate_run(uint64_t *regs, int param, void (*fn)(uint64_t *, int));
void dm_nvn_canvas_size(int *w, int *h);
void dm_nvn_window_size(int *w, int *h);
int dm_opmode(void);

#define K_BUILD_ID "aeea627ddbd1214764dfef5f965e94a2"
#define K_SCREEN_GET_WIDTH 0x17F6530u
#define K_SCREEN_GET_HEIGHT 0x17F6570u
#define K_SCREEN_SET_RESOLUTION 0x17F67D0u
#define K_WARMUP_FRAMES 120
#define K_RECHECK_FRAMES 60
#define K_MAX_TEXTURE_W 1920
#define K_MAX_TEXTURE_H 1080
#define K_MAX_ATTEMPTS 8

static uint8_t *s_main;
static int g_state, g_frame, g_next_check = K_WARMUP_FRAMES, g_attempts;

static uint8_t *main_base(void) {
    if (!s_main)
        for (int i = 0; i < g_nmod; i++)
            if (!strcmp(g_mod[i].name, "main") && !g_mod[i].nro) { s_main = g_mod[i].base; break; }
    return s_main;
}

#define FN(rva, ret, ...) ((ret (*)(__VA_ARGS__))(s_main + (rva)))

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
        static int logged;
        if (!logged++) LOG("[mod fullscreen-mode] game already renders %dx%d", sw, sh);
        return;
    }
    if (g_attempts >= K_MAX_ATTEMPTS) return;
    g_attempts++;
    FN(K_SCREEN_SET_RESOLUTION, void, int, int, uint8_t, const void *)(tw, th, 1, NULL);
    int pw, ph;
    dm_nvn_window_size(&pw, &ph);
    LOG("[mod fullscreen-mode] frame %d: Screen %dx%d -> SetResolution %dx%d (attempt %d); picture last presented %dx%d", g_frame, sw, sh, tw, th, g_attempts, pw, ph);
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

static void on_frame_pre(uint64_t *r, int p) { if (mod_enabled()) dm_mod_gate_run(r, p, frame_run); }

__attribute__((constructor)) static void mod_fullscreen_mode_attach(void) {
    gate_obs_set(MODGATE_FULLSCREEN_MODE_FRAME_PRE, on_frame_pre);
}
