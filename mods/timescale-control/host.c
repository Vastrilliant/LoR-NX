// Based on Punai's Timescale Control (Steam Workshop 3276965287)
#include "dm.h"
#include "hle_mods_gen.h"

void dm_mod_gate_run(uint64_t *regs, int param, void (*fn)(uint64_t *, int));

#define K_BUILD_ID "aeea627ddbd1214764dfef5f965e94a2"
#define K_RESOLVE_ICALL 0x32F5B4u
#define F_STAGE_PHASE 0x70
#define PHASE_FIRST_COMBAT 6
#define PHASE_LAST_COMBAT_A 8
#define PHASE_FIRST_COMBAT_B 11
#define PHASE_LAST_COMBAT_B 26
#define COMBAT_STALE_FRAMES 30
#define SPEED_MIN 1.0f
#define SPEED_MAX 4.0f

static uint8_t *s_main;
static int g_state;
static unsigned g_frame, g_tick_frame;
static int g_combat, g_active;
static float g_speed = 1.0f, g_game = 1.0f, g_last = 1.0f;
static float (*g_get)(void);
static void (*g_set)(float);

static uint8_t *main_base(void) {
    if (!s_main)
        for (int i = 0; i < g_nmod; i++)
            if (!strcmp(g_mod[i].name, "main") && !g_mod[i].nro) { s_main = g_mod[i].base; break; }
    return s_main;
}

static int is_combat_phase(int phase) {
    return (phase >= PHASE_FIRST_COMBAT && phase <= PHASE_LAST_COMBAT_A) || (phase >= PHASE_FIRST_COMBAT_B && phase <= PHASE_LAST_COMBAT_B);
}

static float want_speed(void) {
    const char *v = getenv("DM_MOD_TIMESCALE_SPEED");
    double s = v && v[0] ? atof(v) : 2.0;
    if (s < SPEED_MIN) s = SPEED_MIN;
    if (s > SPEED_MAX) s = SPEED_MAX;
    return (float)s;
}

static int mod_enabled(void) {
    static int on = -1;
    if (on < 0) {
        const char *v = getenv("DM_MOD_TIMESCALE");
        on = v && v[0] == '1';
        LOG("[mod timescale-control] %s (DM_MOD_TIMESCALE=%s)", on ? "on" : "off", v ? v : "unset");
    }
    return on;
}

static int start(void) {
    if (g_state > 0) return 1;
    if (g_state < 0 || !main_base()) return 0;
    if (strncmp(dm_main_build(), K_BUILD_ID, 32) != 0) { g_state = -1; LOG("[mod timescale-control] wrong build, mod stays out"); return 0; }
    void *(*resolve)(const char *) = (void *(*)(const char *))(s_main + K_RESOLVE_ICALL);
    void *get = resolve("UnityEngine.Time::get_timeScale()");
    if (!get) get = resolve("UnityEngine.Time::get_timeScale");
    void *set = resolve("UnityEngine.Time::set_timeScale(System.Single)");
    if (!set) set = resolve("UnityEngine.Time::set_timeScale");
    if (!get || !set) { g_state = -1; LOG("[mod timescale-control] Time.timeScale icalls not found (get %p, set %p), mod stays out", get, set); return 0; }
    g_get = (float (*)(void))get;
    g_set = (void (*)(float))set;
    g_speed = want_speed();
    g_state = 1;
    LOG("[mod timescale-control] on: combat speed x%.2f", g_speed);
    return 1;
}

static void step(int combat) {
    float t = g_get();
    if (!(g_active && t == g_last)) g_game = t;
    int want = combat && g_speed > SPEED_MIN;
    float target = want ? g_game * g_speed : g_game;
    if (target != t) {
        g_set(target);
        static int logged;
        if (want != g_active && logged++ < 40) LOG("[mod timescale-control] combat %s: timeScale %.3f -> %.3f", want ? "speed-up on" : "speed-up off", t, target);
    }
    g_last = target;
    g_active = want;
}

static void fixed_run(uint64_t *r, int param) {
    (void)param;
    if (!start()) return;
    void *stage = (void *)r[0];
    if (!stage) return;
    g_combat = is_combat_phase(*(const int *)((uintptr_t)stage + F_STAGE_PHASE));
    g_tick_frame = g_frame;
    step(g_combat);
}

static void frame_run(uint64_t *r, int param) {
    (void)r; (void)param;
    if (!start()) return;
    g_frame++;
    step(g_combat && g_frame - g_tick_frame <= COMBAT_STALE_FRAMES);
}

static void on_fixed_update_pre(uint64_t *r, int p) { if (mod_enabled()) dm_mod_gate_run(r, p, fixed_run); }
static void on_frame_pre(uint64_t *r, int p) { if (mod_enabled()) dm_mod_gate_run(r, p, frame_run); }

__attribute__((constructor)) static void mod_timescale_control_attach(void) {
    gate_obs_set(MODGATE_TIMESCALE_CONTROL_FIXED_UPDATE_PRE, on_fixed_update_pre);
    gate_obs_set(MODGATE_TIMESCALE_CONTROL_FRAME_PRE, on_frame_pre);
}
