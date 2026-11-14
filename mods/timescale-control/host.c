// Based on Punai's Timescale Control (Steam Workshop 3276965287)
#include "dm.h"
#include "hle_mods_gen.h"

#define K_BUILD_ID "aeea627ddbd1214764dfef5f965e94a2"
#define F_STAGE_PHASE 0x70
#define REGS_Q0_OFFSET 256
#define PHASE_FIRST_COMBAT 6
#define PHASE_LAST_COMBAT_A 8
#define PHASE_FIRST_COMBAT_B 11
#define PHASE_LAST_COMBAT_B 26
#define COMBAT_STALE_CALLS 30
#define SPEED_MIN 1.0f
#define SPEED_MAX 4.0f

static int g_state;
static int g_combat, g_active, g_last_phase = -1, g_phase_logs;
static unsigned g_calls, g_tick_calls;
static float g_speed = 1.0f;

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
    if (g_state < 0) return 0;
    if (strncmp(dm_main_build(), K_BUILD_ID, 32) != 0) { g_state = -1; LOG("[mod timescale-control] wrong build, mod stays out"); return 0; }
    g_speed = want_speed();
    g_state = 1;
    LOG("[mod timescale-control] on: combat speed x%.2f", g_speed);
    return 1;
}

static void on_fixed_update_pre(uint64_t *r, int p) {
    (void)p;
    if (!mod_enabled() || !start()) return;
    const uint8_t *stage = (const uint8_t *)r[0];
    if (!stage) return;
    int phase = *(const int *)(stage + F_STAGE_PHASE);
    g_combat = is_combat_phase(phase);
    g_tick_calls = g_calls;
    if (phase != g_last_phase) {
        g_last_phase = phase;
        if (g_phase_logs++ < 60) LOG("[mod timescale-control] battle phase %d (%s)", phase, g_combat ? "combat" : "not combat");
    }
}

static void on_set_timescale_pre(uint64_t *r, int p) {
    (void)p;
    if (!mod_enabled() || !start()) return;
    g_calls++;
    int want = g_combat && g_calls - g_tick_calls <= COMBAT_STALE_CALLS && g_speed > SPEED_MIN;
    float *s0 = (float *)((uint8_t *)r + REGS_Q0_OFFSET);
    float game = *s0;
    if (want) *s0 = game * g_speed;
    if (want != g_active) {
        g_active = want;
        static int logged;
        if (logged++ < 40) LOG("[mod timescale-control] combat speed-up %s (game timeScale %.3f, set %.3f)", want ? "on" : "off", game, want ? game * g_speed : game);
    }
}

__attribute__((constructor)) static void mod_timescale_control_attach(void) {
    gate_obs_set(MODGATE_TIMESCALE_CONTROL_FIXED_UPDATE_PRE, on_fixed_update_pre);
    gate_obs_set(MODGATE_TIMESCALE_CONTROL_SET_TIMESCALE_PRE, on_set_timescale_pre);
}
