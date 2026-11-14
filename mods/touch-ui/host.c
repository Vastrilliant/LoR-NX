#include "dm.h"
#include "front.h"
#include "dm_input.h"
#include "hle_mods_gen.h"
#include <math.h>
#include <time.h>

void dm_mod_gate_run(uint64_t *regs, int param, void (*fn)(uint64_t *, int));
int dm_touchscreen_mode(void);
int dm_opmode(void);
void dm_nvn_window_size(int *w, int *h);

#define INPUT_PREFIX "UnityEngine.Input::"
#define CURSOR_PREFIX "UnityEngine.Cursor::"

static int mod_enabled(void) {
    static int on = -1;
    if (on < 0) {
        const char *v = getenv("DM_MOD_TOUCHUI");
        on = v && v[0] == '1';
        LOG("[mod touch-ui] %s (DM_MOD_TOUCHUI=%s)", on ? "on" : "off", v ? v : "unset");
    }
    return on;
}

static void screen_size(int *w, int *h) {
    dm_nvn_window_size(w, h);
    if (*w > 0 && *h > 0) return;
    int handheld = dm_opmode() == 0;
    *w = handheld ? 1280 : 1920;
    *h = handheld ? 720 : 1080;
}

#define K_BUILD_ID "aeea627ddbd1214764dfef5f965e94a2"
#define K_MANAGER_TABLE 0x54D4828u
#define K_INPUT_MANAGER_SLOT 1u
#define F_MOUSE_X 0xC8u
#define F_MOUSE_Y 0xCCu
#define K_RESOLVE_ICALL 0x32F5B4u

static uint8_t *main_base(void) {
    static uint8_t *base;
    if (!base)
        for (int i = 0; i < g_nmod; i++)
            if (!strcmp(g_mod[i].name, "main") && !g_mod[i].nro) { base = g_mod[i].base; break; }
    return base;
}

#define FN(rva, ret, ...) ((ret (*)(__VA_ARGS__))(main_base() + (rva)))

#define K_SR_INIT_POTENTIAL_DRAG 0x2971520u
#define K_SR_BEGIN_DRAG 0x29715A0u
#define K_SR_END_DRAG 0x29716F0u
#define K_SR_DRAG 0x2971720u
#define K_COMPONENT_GET_TRANSFORM 0x19AE1D0u
#define K_TRANSFORM_GET_PARENT 0x19BC440u
#define K_TRANSFORM_GET_LOSSY_SCALE 0x19BFA20u
#define F_SR_INERTIA 0x2Cu
#define F_SR_DECELERATION 0x30u
#define F_SR_POINTER_START 0x68u
#define F_SR_CONTENT_START 0x70u
#define F_SR_VELOCITY 0xB0u
#define F_SR_VIEW_RECT 0x78u
#define F_SR_DRAGGING 0xB8u
#define F_EQUIP_LIST_SCROLL_RECT 0x28u
#define F_SR_PREV_POSITION 0xBCu
#define F_EVENT_ELIGIBLE 0xF8u
#define F_EVENT_POSITION 0x100u
#define F_EVENT_DELTA 0x108u
#define DEFAULT_DECELERATION 0.135f
#define MAX_RECTS 64
#define MAX_DEPTH 32
#define MAX_RECYCLING 8
#define N_SAMPLES 8
#define STALE_DRAG_MS 250u
#define RELEASE_WINDOW_MS 120u
#define MIN_RELEASE_SPAN_S 0.016f
#define MAX_LOGGED_PER_LIST 50
#define MIN_GLIDE_SPEED 20.0f
#define MAX_GLIDE_GAP_MS 100u

static void *s_rects[MAX_RECTS];
static int s_nrects;
static void *s_drag_trigger;
static void *s_drag_rect;

static void *ptr_at(void *self, uintptr_t off) { return self ? *(void *const *)((uintptr_t)self + off) : NULL; }

static void rect_add(void *rect) {
    for (int i = 0; i < s_nrects; i++) if (s_rects[i] == rect) return;
    if (s_nrects < MAX_RECTS) s_rects[s_nrects++] = rect;
}

static void rect_del(void *rect) {
    for (int i = 0; i < s_nrects; i++)
        if (s_rects[i] == rect) { s_rects[i] = s_rects[--s_nrects]; break; }
    if (s_drag_rect == rect) { s_drag_rect = NULL; s_drag_trigger = NULL; }
}

static void *tf_of(void *component) { return component ? FN(K_COMPONENT_GET_TRANSFORM, void *, void *, void *)(component, NULL) : NULL; }
static void *parent_of(void *tf) { return tf ? FN(K_TRANSFORM_GET_PARENT, void *, void *, void *)(tf, NULL) : NULL; }

static void *rect_above(void *trigger) {
    void *t = parent_of(tf_of(trigger));
    for (int depth = 0; t && depth < MAX_DEPTH; depth++, t = parent_of(t))
        for (int i = 0; i < s_nrects; i++)
            if (tf_of(s_rects[i]) == t) return s_rects[i];
    return NULL;
}

static void *s_fwd_trigger;
static void *s_fwd_event;
static int s_fwd_kind;

static void forward_run(uint64_t *r, int p) {
    (void)r; (void)p;
    void *rect = (s_drag_trigger == s_fwd_trigger) ? s_drag_rect : NULL;
    if (!rect) rect = rect_above(s_fwd_trigger);
    static int n_log;
    if (n_log++ < 40) LOG("[mod touch-ui] drag step %d on trigger %p: list %p", s_fwd_kind, s_fwd_trigger, rect);
    if (!rect) { s_drag_trigger = NULL; s_drag_rect = NULL; return; }
    switch (s_fwd_kind) {
    case 3:
        s_drag_trigger = s_fwd_trigger;
        s_drag_rect = rect;
        FN(K_SR_INIT_POTENTIAL_DRAG, void, void *, void *, void *)(rect, s_fwd_event, NULL);
        break;
    case 4:
        s_drag_trigger = s_fwd_trigger;
        s_drag_rect = rect;
        FN(K_SR_BEGIN_DRAG, void, void *, void *, void *)(rect, s_fwd_event, NULL);
        *(uint8_t *)((uintptr_t)s_fwd_event + F_EVENT_ELIGIBLE) = 0;
        break;
    case 5:
        FN(K_SR_DRAG, void, void *, void *, void *)(rect, s_fwd_event, NULL);
        break;
    case 6:
        FN(K_SR_END_DRAG, void, void *, void *, void *)(rect, s_fwd_event, NULL);
        s_drag_trigger = NULL;
        s_drag_rect = NULL;
        break;
    default:
        break;
    }
}

typedef struct { float x, y, z; } V3;

static struct recycling {
    void *rect;
    int have_prev;
    int n_logged;
    float prev_x, prev_y;
    float glide_v;
    uint64_t glide_t;
    int n, head;
    uint64_t t[N_SAMPLES];
    float y[N_SAMPLES];
} s_rec[MAX_RECYCLING];
static int s_nrec;
static void *s_rebase_rect;
static void *s_rebase_event;

static uint64_t now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}

static struct recycling *rec_find(void *rect) {
    for (int i = 0; i < s_nrec; i++) if (s_rec[i].rect == rect) return &s_rec[i];
    return NULL;
}

static void rec_add(void *rect) {
    if (!rect || rec_find(rect) || s_nrec >= MAX_RECYCLING) return;
    s_rec[s_nrec].rect = rect;
    s_nrec++;
    LOG("[mod touch-ui] list %p recycles its slots while it scrolls: its drags are applied step by step", rect);
}

static void rec_sample(struct recycling *rc, uint64_t t, float y) {
    rc->head = (rc->head + 1) % N_SAMPLES;
    rc->t[rc->head] = t;
    rc->y[rc->head] = y;
    if (rc->n < N_SAMPLES) rc->n++;
}

static void rebase_run(uint64_t *r, int p) {
    (void)r; (void)p;
    FN(K_SR_BEGIN_DRAG, void, void *, void *, void *)(s_rebase_rect, s_rebase_event, NULL);
}

static void log_list_drag(void *rect, void *event, struct recycling *rc) {
    static void *other_rect[MAX_RECYCLING];
    static int other_n[MAX_RECYCLING];
    int *n_logged = rc ? &rc->n_logged : NULL;
    if (!n_logged) {
        for (int i = 0; i < MAX_RECYCLING; i++) {
            if (other_rect[i] == rect || !other_rect[i]) { other_rect[i] = rect; n_logged = &other_n[i]; break; }
        }
    }
    if (!n_logged || (*n_logged)++ >= MAX_LOGGED_PER_LIST) return;
    const float *start = (const float *)((uintptr_t)rect + F_SR_POINTER_START);
    const float *content = (const float *)((uintptr_t)rect + F_SR_CONTENT_START);
    const float *vel = (const float *)((uintptr_t)rect + F_SR_VELOCITY);
    const float *prev = (const float *)((uintptr_t)rect + F_SR_PREV_POSITION);
    const float *pos = (const float *)((uintptr_t)event + F_EVENT_POSITION);
    const float *delta = (const float *)((uintptr_t)event + F_EVENT_DELTA);
    LOG("[mod touch-ui] list %p%s drag: pointer (%.1f,%.1f) delta (%.1f,%.1f); pointerStart (%.1f,%.1f) contentStart (%.1f,%.1f) prev (%.1f,%.1f) velocity (%.1f,%.1f)",
        rect, rc ? " (recycling)" : "", pos[0], pos[1], delta[0], delta[1], start[0], start[1], content[0], content[1], prev[0], prev[1], vel[0], vel[1]);
}

static void recycling_drag(struct recycling *rc, void *rect, void *event, uint64_t *r, int p) {
    float *pos = (float *)((uintptr_t)event + F_EVENT_POSITION);
    uint64_t t = now_ms();
    uint64_t last = rc->n ? rc->t[rc->head] : 0;
    if (rc->have_prev && t - last <= STALE_DRAG_MS && *(uint8_t *)((uintptr_t)rect + F_SR_DRAGGING)) {
        float cur_x = pos[0], cur_y = pos[1];
        pos[0] = rc->prev_x;
        pos[1] = rc->prev_y;
        s_rebase_rect = rect;
        s_rebase_event = event;
        dm_mod_gate_run(r, p, rebase_run);
        pos[0] = cur_x;
        pos[1] = cur_y;
    } else {
        rc->n = 0;
    }
    rc->prev_x = pos[0];
    rc->prev_y = pos[1];
    rc->have_prev = 1;
    rec_sample(rc, t, pos[1]);
}

static void recycling_release(struct recycling *rc, void *rect) {
    static int n_log;
    float v = 0.0f;
    uint64_t t = now_ms();
    if (rc->have_prev && rc->n >= 2 && t - rc->t[rc->head] <= RELEASE_WINDOW_MS) {
        int old = rc->head;
        for (int k = 1; k < rc->n; k++) {
            int i = (rc->head - k + N_SAMPLES) % N_SAMPLES;
            if (rc->t[rc->head] - rc->t[i] > RELEASE_WINDOW_MS) break;
            old = i;
        }
        float span = (float)(rc->t[rc->head] - rc->t[old]) / 1000.0f;
        void *view = ptr_at(rect, F_SR_VIEW_RECT);
        V3 scale = view ? FN(K_TRANSFORM_GET_LOSSY_SCALE, V3, void *, void *)(view, NULL) : (V3){1.0f, 1.0f, 1.0f};
        if (span >= MIN_RELEASE_SPAN_S && scale.y > 0.0f) v = (rc->y[rc->head] - rc->y[old]) / span / scale.y;
    }
    float *velocity = (float *)((uintptr_t)rect + F_SR_VELOCITY);
    if (n_log++ < 30) LOG("[mod touch-ui] list %p released: velocity (%.1f,%.1f) from the drag; the list had (%.1f,%.1f)", rect, 0.0, (double)v, (double)velocity[0], (double)velocity[1]);
    velocity[0] = 0.0f;
    velocity[1] = v;
    rc->glide_v = v;
    rc->glide_t = t;
    rc->have_prev = 0;
    rc->n = 0;
}

static void recycling_late(struct recycling *rc, void *rect) {
    static int n_log;
    float *velocity = (float *)((uintptr_t)rect + F_SR_VELOCITY);
    uint64_t t = now_ms();
    if (*(uint8_t *)((uintptr_t)rect + F_SR_DRAGGING)) { rc->glide_v = 0.0f; return; }
    float v = velocity[1];
    if (v == 0.0f && fabsf(rc->glide_v) > MIN_GLIDE_SPEED && t - rc->glide_t <= MAX_GLIDE_GAP_MS) {
        float dt = (float)(t - rc->glide_t) / 1000.0f;
        v = rc->glide_v * powf(*(float *)((uintptr_t)rect + F_SR_DECELERATION), dt);
        velocity[1] = v;
        if (n_log++ < 30) LOG("[mod touch-ui] list %p: the shift of its slots stopped its glide at %.1f, which goes on at %.1f", rect, (double)rc->glide_v, (double)v);
    }
    rc->glide_v = v;
    rc->glide_t = t;
}

static void on_scroll_ui(uint64_t *r, int p) {
    if (!mod_enabled() || !dm_touchscreen_mode() || !main_base()) return;
    void *self = (void *)(uintptr_t)r[0];
    if (!self) return;
    switch (p) {
    case 1: {
        rect_add(self);
        *(uint8_t *)((uintptr_t)self + F_SR_INERTIA) = 1;
        float *deceleration = (float *)((uintptr_t)self + F_SR_DECELERATION);
        if (*deceleration < 0.05f) *deceleration = DEFAULT_DECELERATION;
        break;
    }
    case 2:
        rect_del(self);
        break;
    case 3: case 4: case 5: case 6:
        s_fwd_trigger = self;
        s_fwd_event = (void *)(uintptr_t)r[1];
        s_fwd_kind = p;
        if (s_fwd_event) dm_mod_gate_run(r, p, forward_run);
        break;
    case 7: {
        void *event = (void *)(uintptr_t)r[1];
        if (!event) break;
        struct recycling *rc = rec_find(self);
        log_list_drag(self, event, rc);
        if (rc) recycling_drag(rc, self, event, r, p);
        break;
    }
    case 8:
        rec_add(ptr_at(self, F_EQUIP_LIST_SCROLL_RECT));
        break;
    case 9: {
        struct recycling *rc = rec_find(self);
        if (rc) recycling_release(rc, self);
        break;
    }
    case 10: {
        struct recycling *rc = rec_find(self);
        if (rc) rc->glide_v = 0.0f;
        break;
    }
    case 11: {
        struct recycling *rc = rec_find(self);
        if (rc) recycling_late(rc, self);
        break;
    }
    default:
        break;
    }
}

static void push_engine_position(float x, float y) {
    static int build_ok = -1;
    if (build_ok < 0) build_ok = strncmp(dm_main_build(), K_BUILD_ID, 32) == 0;
    if (!build_ok) return;
    uint8_t *base = main_base();
    if (!base) return;
    uint8_t *mgr = *(uint8_t *volatile *)(base + K_MANAGER_TABLE + 8u * K_INPUT_MANAGER_SLOT);
    if (!mgr) return;
    volatile float *pos = (volatile float *)(mgr + F_MOUSE_X);
    pos[0] = x;
    *(volatile float *)(mgr + F_MOUSE_Y) = y;
    static int logged;
    if (!logged++) LOG("[mod touch-ui] engine mouse position at InputManager %p fed from the touchpad", (void *)mgr);
}

#define K_CARD_SHOW_DETAIL 0x1FD6D10u
#define F_CARD_ENTERED 0x133u

extern int (*dm_press_owner)(void *card, void *event);

static void *s_press_card;

static void page_hover_run(uint64_t *r, int p) {
    (void)p;
    if (dm_press_owner && dm_press_owner(s_press_card, (void *)(uintptr_t)r[1])) return;
    static int logged;
    ((void (*)(void *, void *))(main_base() + K_CARD_SHOW_DETAIL))(s_press_card, NULL);
    if (logged++ < 5) LOG("[mod touch-ui] combat page %p pressed before its hover: ShowDetail run first", s_press_card);
}

static void on_page_press(uint64_t *r, int p) {
    if (!mod_enabled() || !dm_touchscreen_mode() || !main_base()) return;
    static int build_ok = -1;
    if (build_ok < 0) build_ok = strncmp(dm_main_build(), K_BUILD_ID, 32) == 0;
    if (!build_ok) return;
    uint8_t *card = (uint8_t *)(uintptr_t)r[0];
    if (!card || card[F_CARD_ENTERED]) return;
    s_press_card = card;
    dm_mod_gate_run(r, p, page_hover_run);
}

static int (*g_unity_frame_count)(void);

static uint64_t game_frame(void) {
    static int tried;
    if (!tried) {
        tried = 1;
        uint8_t *base = main_base();
        if (base && strncmp(dm_main_build(), K_BUILD_ID, 32) == 0) {
            void *(*resolve)(const char *) = (void *(*)(const char *))(base + K_RESOLVE_ICALL);
            void *fn = resolve("UnityEngine.Time::get_frameCount()");
            if (!fn) fn = resolve("UnityEngine.Time::get_frameCount");
            if (fn) {
                g_unity_frame_count = (int (*)(void))fn;
                LOG("[mod touch-ui] mouse edges follow Time.frameCount (native %p)", fn);
            } else {
                LOG("[mod touch-ui] Time.frameCount not found, mouse edges follow the present count");
            }
        }
    }
    if (g_unity_frame_count) return (uint64_t)(uint32_t)g_unity_frame_count();
    return dm_nvn_frame_count();
}

#define F_HAND_ACTIVE 0x40u
#define HAND_LIVE_MS 250u
#define DOUBLE_GAP_MS 450u
#define DOUBLE_TAP_MS 300u
#define DOUBLE_DICE_MS 500u
#define DOUBLE_RADIUS 32.0f

static void *s_hand;
static uint64_t s_hand_ms;
static uint64_t s_dice_ms;

static struct {
    int valid, pressed;
    uint64_t down_ms, up_ms;
    float x, y;
} s_tap;
static int s_as_right, s_as_right_end;

static int hand_active(void) {
    if (!s_hand || now_ms() - s_hand_ms > HAND_LIVE_MS) return 0;
    return *(volatile uint8_t *)((uintptr_t)s_hand + F_HAND_ACTIVE) != 0;
}

static void track_double(const dm_mouse_frame_t *f) {
    if (s_as_right_end) s_as_right = s_as_right_end = 0;
    uint64_t t = now_ms();
    float x = f->nx * 1280.0f, y = f->ny * 720.0f;
    if (f->down & DM_MOUSE_LEFT) {
        int dice = t - s_dice_ms <= DOUBLE_DICE_MS;
        int hand = hand_active();
        int dbl = s_tap.valid && t - s_tap.up_ms <= DOUBLE_GAP_MS && hypotf(x - s_tap.x, y - s_tap.y) <= DOUBLE_RADIUS && (dice || hand);
        s_tap.valid = 0;
        s_tap.pressed = !dbl;
        if (dbl) {
            s_as_right = 1;
            static int n_log;
            if (n_log++ < 40) LOG("[mod touch-ui] double tap at (%.0f,%.0f) on %s: sent as the right button", x, y, dice ? "a speed dice" : "the shown hand");
        } else {
            s_tap.down_ms = t;
            s_tap.x = x;
            s_tap.y = y;
        }
    }
    if (f->up & DM_MOUSE_LEFT) {
        if (s_as_right) {
            s_as_right_end = 1;
        } else if (s_tap.pressed) {
            s_tap.pressed = 0;
            if (t - s_tap.down_ms <= DOUBLE_TAP_MS) {
                s_tap.valid = 1;
                s_tap.up_ms = t;
            }
        }
    }
}

static uint32_t left_as_right(uint32_t mask) {
    return (mask & ~(uint32_t)DM_MOUSE_LEFT) | ((mask & DM_MOUSE_LEFT) ? DM_MOUSE_RIGHT : 0u);
}

static dm_mouse_frame_t double_as_right(dm_mouse_frame_t f) {
    if (!s_as_right) return f;
    f.held = left_as_right(f.held);
    f.down = left_as_right(f.down);
    f.up = left_as_right(f.up);
    return f;
}

static dm_mouse_frame_t frame_now(void) {
    static uint64_t pushed = ~0ull;
    uint64_t frame = game_frame();
    dm_mouse_frame_t f;
    dm_mouse_frame(frame, &f);
    if (pushed != frame) {
        pushed = frame;
        int w, h;
        screen_size(&w, &h);
        push_engine_position(f.nx * (float)(w - 1), (1.0f - f.ny) * (float)(h - 1));
        static uint32_t last_held;
        static int logged_changes;
        if (f.held != last_held && logged_changes < 200) {
            logged_changes++;
            LOG("[mod touch-ui] frame %llu buttons held %#x down %#x up %#x at (%.0f,%.0f)", (unsigned long long)frame, f.held, f.down, f.up, f.nx * (float)(w - 1), (1.0f - f.ny) * (float)(h - 1));
        }
        last_held = f.held;
        track_double(&f);
    }
    return double_as_right(f);
}

static uint32_t button_mask(int button) {
    return button == 0 ? DM_MOUSE_LEFT : button == 1 ? DM_MOUSE_RIGHT : 0;
}

static int host_get_mouse_button(int button) { return (frame_now().held & button_mask(button)) != 0; }
static int host_get_mouse_button_down(int button) { return (frame_now().down & button_mask(button)) != 0; }
static int host_get_mouse_button_up(int button) { return (frame_now().up & button_mask(button)) != 0; }
static int host_get_mouse_present(void) { return 1; }

static void host_get_mouse_position(float *out) {
    dm_mouse_frame_t f = frame_now();
    int w, h;
    screen_size(&w, &h);
    out[0] = f.nx * (float)(w - 1);
    out[1] = (1.0f - f.ny) * (float)(h - 1);
    out[2] = 0.0f;
}

static void host_get_mouse_scroll_delta(float *out) {
    out[0] = 0.0f;
    out[1] = (float)frame_now().wheel;
}

static float (*g_real_get_axis)(void *);
static float (*g_real_get_axis_raw)(void *);

static int is_scroll_axis(const void *name) {
    static const char k[] = "Mouse ScrollWheel";
    if (!name) return 0;
    const uint8_t *s = (const uint8_t *)name;
    if (*(const int32_t *)(s + 0x10) != (int32_t)(sizeof k - 1)) return 0;
    const uint16_t *c = (const uint16_t *)(s + 0x14);
    for (size_t i = 0; i < sizeof k - 1; i++)
        if (c[i] != (uint16_t)k[i]) return 0;
    return 1;
}

static float scroll_axis(void) {
    int notches = frame_now().wheel;
    static int logged;
    if (notches && !logged++) LOG("[mod touch-ui] Input.GetAxis(\"Mouse ScrollWheel\") answered from the touchpad wheel (%d notches)", notches);
    return 0.1f * (float)notches;
}

static float host_get_axis(void *name) {
    if (is_scroll_axis(name)) return scroll_axis();
    return g_real_get_axis ? g_real_get_axis(name) : 0.0f;
}

static float host_get_axis_raw(void *name) {
    if (is_scroll_axis(name)) return scroll_axis();
    return g_real_get_axis_raw ? g_real_get_axis_raw(name) : 0.0f;
}

static int usage_of_keycode(int code) {
    switch (code) {
    case 27: return 41;
    case 32: return 44;
    case 303: case 304: return 225;
    case 305: case 306: return 224;
    case 307: case 308: return 226;
    default: return -1;
    }
}

static dm_key_frame_t key_frame_now(void) {
    dm_key_frame_t k;
    dm_key_frame(game_frame(), &k);
    return k;
}

static int key_bit(const uint32_t *set, int code) {
    int u = usage_of_keycode(code);
    return u >= 0 && ((set[u / 32] >> (u % 32)) & 1u);
}

static int host_get_key(int code) { dm_key_frame_t k = key_frame_now(); return key_bit(k.held, code); }
static int host_get_key_down(int code) { dm_key_frame_t k = key_frame_now(); return key_bit(k.down, code); }
static int host_get_key_up(int code) { dm_key_frame_t k = key_frame_now(); return key_bit(k.up, code); }

static int host_get_any_key_down(void) {
    dm_key_frame_t k = key_frame_now();
    for (int i = 0; i < 8; i++) if (k.down[i]) return 1;
    return (frame_now().down & (DM_MOUSE_LEFT | DM_MOUSE_RIGHT)) != 0;
}

static const struct { const char *name; void (*fn)(void); void **real; } k_icalls[] = {
    { "GetKeyInt", (void (*)(void))host_get_key },
    { "GetKeyDownInt", (void (*)(void))host_get_key_down },
    { "GetKeyUpInt", (void (*)(void))host_get_key_up },
    { "get_anyKeyDown", (void (*)(void))host_get_any_key_down },
    { "GetMouseButton", (void (*)(void))host_get_mouse_button },
    { "GetMouseButtonDown", (void (*)(void))host_get_mouse_button_down },
    { "GetMouseButtonUp", (void (*)(void))host_get_mouse_button_up },
    { "get_mousePresent", (void (*)(void))host_get_mouse_present },
    { "get_mousePosition_Injected", (void (*)(void))host_get_mouse_position },
    { "get_mouseScrollDelta_Injected", (void (*)(void))host_get_mouse_scroll_delta },
    { "GetAxis", (void (*)(void))host_get_axis, (void **)&g_real_get_axis },
    { "GetAxisRaw", (void (*)(void))host_get_axis_raw, (void **)&g_real_get_axis_raw },
};

// Pointer Mode "Touch" (the launcher's DM_POINTER_MODE=touch, with Touchscreen Mode): the cursor is the finger, so the game's own cursor (CursorManager draws its textures with
// UnityEngine.Cursor.SetCursor and shows them with Cursor.visible) must stay hidden. Both natives are replaced when the icall resolves, in Touch mode only, so Trackpad mode is untouched:
// SetCursor_Injected(texture, hotspot, mode) does nothing (no cursor image is ever set), and set_visible(bool) hands the engine false whatever the game asks for.
// Read at the first icall, like the mod's own switch: the launcher's choices are not in the environment yet when constructors run.
static int pointer_touch(void) {
    static int on = -1;
    if (on < 0) {
        const char *v = getenv("DM_POINTER_MODE");
        on = v && !strcmp(v, "touch");
        LOG("[mod touch-ui] pointer mode %s (DM_POINTER_MODE=%s)", on ? "touch: the game's cursor is hidden" : "trackpad", v ? v : "unset");
    }
    return on;
}

static void (*g_real_set_visible)(int);

static void host_cursor_set_cursor(void *texture, void *hotspot, int mode) { (void)texture; (void)hotspot; (void)mode; }
static void host_cursor_set_visible(int visible) {
    (void)visible;
    if (g_real_set_visible) g_real_set_visible(0);
}

static int on_cursor_icall(const char *name, uint64_t *r) {
    const char *method = name + sizeof CURSOR_PREFIX - 1;
    if (!strncmp(method, "SetCursor_Injected", 18) && (method[18] == '\0' || method[18] == '(')) {
        LOG("[mod touch-ui] %s: icall %#llx replaced, the game's cursor image is never set", name, (unsigned long long)r[0]);
        r[0] = (uint64_t)(uintptr_t)host_cursor_set_cursor;
        return 1;
    }
    if (!strncmp(method, "set_visible", 11) && (method[11] == '\0' || method[11] == '(')) {
        LOG("[mod touch-ui] %s: icall %#llx replaced, the game's cursor stays hidden", name, (unsigned long long)r[0]);
        g_real_set_visible = (void (*)(int))(uintptr_t)r[0];
        r[0] = (uint64_t)(uintptr_t)host_cursor_set_visible;
        return 1;
    }
    return 0;
}

#define K_SINGLETON_GET_INSTANCE 0x2D5C5E0u
#define K_CAMERA_GET_ORTHO 0x17E9450u
#define K_CAMERA_SET_ORTHO 0x17E94A0u
#define K_GATE_A_METHOD 0x51D93E8u
#define K_GATE_B_METHOD 0x51E57D8u
#define F_CAM_MODE 0x78u
#define F_CAM_MAIN 0x70u
#define F_CAM_RETURNING 0xC0u
#define F_CAM_SCROLLABLE 0x109u
#define F_CAM_RESET 0x138u
#define F_CAM_MIN_ORTHO 0x2Cu
#define F_CAM_STD_ORTHO 0x88u
#define F_CAM_ZOOM_SPEED 0x98u
#define F_GATE_A_CHILD 0x58u
#define F_GATE_A_FLAG 0x18u
#define F_GATE_B_FLAG1 0x70u
#define F_GATE_B_FLAG2 0x58u
#define ZOOM_PER_NOTCH 0.1f

static void *s_zoom_cam;
static int s_zoom_notches;

static void *singleton_of(uint32_t method_slot) {
    void **slot = *(void ***)(main_base() + method_slot);
    if (!slot) return NULL;
    return ((void *(*)(void *))(main_base() + K_SINGLETON_GET_INSTANCE))(*slot);
}

static int cam_zoom_blocked(void) {
    uint8_t *a = singleton_of(K_GATE_A_METHOD);
    uint8_t *a_child = a ? *(uint8_t **)(a + F_GATE_A_CHILD) : NULL;
    if (!a_child || a_child[F_GATE_A_FLAG]) return 1;
    uint8_t *b = singleton_of(K_GATE_B_METHOD);
    if (!b || b[F_GATE_B_FLAG1]) return 1;
    b = singleton_of(K_GATE_B_METHOD);
    return !b || b[F_GATE_B_FLAG2];
}

static void cam_zoom_run(uint64_t *r, int p) {
    (void)r; (void)p;
    uint8_t *cam = s_zoom_cam;
    void *main_cam = *(void **)(cam + F_CAM_MAIN);
    if (!main_cam || cam_zoom_blocked()) return;
    float ortho = ((float (*)(void *, void *))(main_base() + K_CAMERA_GET_ORTHO))(main_cam, NULL);
    float lo = *(float *)(cam + F_CAM_MIN_ORTHO), hi = *(float *)(cam + F_CAM_STD_ORTHO);
    float next = ortho - (float)s_zoom_notches * ZOOM_PER_NOTCH * *(float *)(cam + F_CAM_ZOOM_SPEED);
    if (next > hi) next = hi;
    if (next < lo) next = lo;
    static int logged;
    if (logged++ < 10) LOG("[mod touch-ui] combat camera zoom %d notches: orthographic size %.3f -> %.3f (min %.3f, max %.3f)", s_zoom_notches, ortho, next, lo, hi);
    ((void (*)(void *, float, void *))(main_base() + K_CAMERA_SET_ORTHO))(main_cam, next, NULL);
}

static void on_cam_late(uint64_t *r, int p) {
    if (!mod_enabled() || !dm_touchscreen_mode() || !main_base()) return;
    static int build_ok = -1;
    if (build_ok < 0) build_ok = strncmp(dm_main_build(), K_BUILD_ID, 32) == 0;
    if (!build_ok) return;
    uint8_t *cam = (uint8_t *)(uintptr_t)r[0];
    if (!cam || *(int *)(cam + F_CAM_MODE) != 0 || cam[F_CAM_RETURNING] || cam[F_CAM_RESET] || !cam[F_CAM_SCROLLABLE]) return;
    int notches = frame_now().wheel;
    if (!notches) return;
    s_zoom_cam = cam;
    s_zoom_notches = notches;
    dm_mod_gate_run(r, p, cam_zoom_run);
}

static void on_hand_update(uint64_t *r) {
    if (!mod_enabled() || !dm_touchscreen_mode()) return;
    s_hand = (void *)(uintptr_t)r[0];
    s_hand_ms = now_ms();
}

static void on_dice_submit(void) {
    if (!mod_enabled() || !dm_touchscreen_mode()) return;
    s_dice_ms = now_ms();
}

#define K_UI_CONTROLLER_GET_INSTANCE 0x16F8D10u
#define F_CONTROLLER_PANELS 0x50u
#define F_CONTROLLER_PREFABS 0x60u
#define F_CONTROLLER_LIVE_PANELS 0x70u
#define F_PANEL_TYPE 0x18u
#define ARRAY_LENGTH 0x18u
#define ARRAY_DATA 0x20u
#define MAX_PANELS 64
#define K_INSTANTIATE_PARENT 0x19B4F00u
#define K_COMPONENT_GET_GAMEOBJECT 0x19AE220u
#define K_GAMEOBJECT_SET_ACTIVE 0x19AFD70u
#define K_BEHAVIOUR_SET_ENABLED 0x19ADFE0u
#define K_TRANSFORM_SET_AS_LAST_SIBLING 0x19BF750u
#define K_RECT_GET_RECT 0x19BA1B0u
#define K_RECT_SET_ANCHOR_MIN 0x19BA350u
#define K_RECT_SET_ANCHOR_MAX 0x19BA4F0u
#define K_RECT_SET_ANCHORED_POSITION 0x19BA690u
#define K_RECT_SET_PIVOT 0x19BA9D0u
#define K_RECT_SET_SIZE_DELTA 0x19BA830u
#define K_INV_SET_PHASE_STATE 0x257C590u
#define PANEL_TYPE_BATTLE_SETTING 10
#define PHASE_STORY 0
#define PHASE_DROP_BOOK_SELECT 1
#define F_BATTLE_SETTING_BACK 0x98u
#define F_INV_PHASE 0x98u
#define F_GRAPHIC_OBJECT_SELECTABLE 0x38u
#define F_SELECTABLE_TRIGGER 0x158u
#define LEAVE_X_FRAC 0.5f
#define LEAVE_Y_FRAC 0.076f
#define LEAVE_PAD_X 1.3f
#define LEAVE_PAD_Y 1.6f
#define LEAVE_FALLBACK_W 220.0f
#define LEAVE_FALLBACK_H 64.0f

typedef struct { float x, y; } V2;
typedef struct { float x, y, w, h; } R4;

static void *s_leave;
static void *s_leave_owner;
static void *s_leave_panel;
static int s_leave_failed;
static int s_leave_shown;
static int s_leave_want;
static int s_leave_pressed;
static float s_leave_half_w, s_leave_half_h;

static int build_matches(void) {
    static int ok = -1;
    if (ok < 0) ok = strncmp(dm_main_build(), K_BUILD_ID, 32) == 0;
    return ok;
}

static int within(void *tf, void *root) {
    for (int depth = 0; tf && depth < MAX_DEPTH; depth++, tf = parent_of(tf))
        if (tf == root) return 1;
    return 0;
}

static void leave_set_active(void *component, int on) {
    void *go = FN(K_COMPONENT_GET_GAMEOBJECT, void *, void *, void *)(component, NULL);
    if (go) FN(K_GAMEOBJECT_SET_ACTIVE, void, void *, int, void *)(go, on, NULL);
}

static void *panel_in(void *controller, uintptr_t field, int type) {
    uint8_t *array = ptr_at(controller, field);
    if (!array) return NULL;
    int64_t n = *(int64_t *)(array + ARRAY_LENGTH);
    if (n <= 0 || n > MAX_PANELS) return NULL;
    void **items = (void **)(array + ARRAY_DATA);
    for (int64_t i = 0; i < n; i++)
        if (items[i] && *(int *)((uintptr_t)items[i] + F_PANEL_TYPE) == type) return items[i];
    return NULL;
}

static void leave_create_run(uint64_t *r, int p) {
    (void)r; (void)p;
    void *panel = s_leave_panel;
    void *controller = FN(K_UI_CONTROLLER_GET_INSTANCE, void *, void *)(NULL);
    void *setting = NULL;
    const char *where = "none";
    if (controller) {
        setting = panel_in(controller, F_CONTROLLER_LIVE_PANELS, PANEL_TYPE_BATTLE_SETTING);
        where = "live panels";
        if (!setting) { setting = panel_in(controller, F_CONTROLLER_PANELS, PANEL_TYPE_BATTLE_SETTING); where = "panels"; }
        if (!setting) { setting = panel_in(controller, F_CONTROLLER_PREFABS, PANEL_TYPE_BATTLE_SETTING); where = "prefabs"; }
    }
    void *source = ptr_at(setting, F_BATTLE_SETTING_BACK);
    void *parent = tf_of(panel);
    if (!source || !parent) {
        s_leave_failed = 1;
        LOG("[mod touch-ui] leave button: no source (controller %p, preparation panel %p, back button %p), left out", controller, setting, source);
        return;
    }
    LOG("[mod touch-ui] leave button: preparation panel %p found in %s, back button %p", setting, where, source);
    R4 src_rect = FN(K_RECT_GET_RECT, R4, void *, void *)(tf_of(source), NULL);
    R4 parent_rect = FN(K_RECT_GET_RECT, R4, void *, void *)(parent, NULL);
    if (parent_rect.w <= 1.0f || parent_rect.h <= 1.0f) {
        LOG("[mod touch-ui] leave button: invitation panel rect %.1fx%.1f is unusable, will retry", parent_rect.w, parent_rect.h);
        return;
    }
    void *clone = FN(K_INSTANTIATE_PARENT, void *, void *, void *, int, void *)(source, parent, 0, NULL);
    if (!clone) {
        s_leave_failed = 1;
        LOG("[mod touch-ui] leave button: Instantiate returned null");
        return;
    }
    void *clone_tf = tf_of(clone);
    void *selectable = ptr_at(clone, F_GRAPHIC_OBJECT_SELECTABLE);
    if (selectable && !within(tf_of(selectable), clone_tf)) {
        s_leave_failed = 1;
        leave_set_active(clone, 0);
        LOG("[mod touch-ui] leave button: the clone's selectable %p is outside the clone, button left hidden", selectable);
        return;
    }
    if (selectable) {
        void *trigger = ptr_at(selectable, F_SELECTABLE_TRIGGER);
        if (trigger) FN(K_BEHAVIOUR_SET_ENABLED, void, void *, int, void *)(trigger, 0, NULL);
        FN(K_BEHAVIOUR_SET_ENABLED, void, void *, int, void *)(selectable, 0, NULL);
    }
    float w = src_rect.w > 1.0f && src_rect.w < parent_rect.w ? src_rect.w : LEAVE_FALLBACK_W;
    float h = src_rect.h > 1.0f && src_rect.h < parent_rect.h ? src_rect.h : LEAVE_FALLBACK_H;
    FN(K_RECT_SET_ANCHOR_MIN, void, void *, float, float, void *)(clone_tf, LEAVE_X_FRAC, 0.0f, NULL);
    FN(K_RECT_SET_ANCHOR_MAX, void, void *, float, float, void *)(clone_tf, LEAVE_X_FRAC, 0.0f, NULL);
    FN(K_RECT_SET_PIVOT, void, void *, float, float, void *)(clone_tf, 0.5f, 0.5f, NULL);
    FN(K_RECT_SET_SIZE_DELTA, void, void *, float, float, void *)(clone_tf, w, h, NULL);
    FN(K_RECT_SET_ANCHORED_POSITION, void, void *, float, float, void *)(clone_tf, 0.0f, LEAVE_Y_FRAC * parent_rect.h, NULL);
    FN(K_TRANSFORM_SET_AS_LAST_SIBLING, void, void *, void *)(clone_tf, NULL);
    leave_set_active(clone, 0);
    s_leave = clone;
    s_leave_owner = panel;
    s_leave_shown = 0;
    s_leave_half_w = w / parent_rect.w * 0.5f * LEAVE_PAD_X;
    s_leave_half_h = h / parent_rect.h * 0.5f * LEAVE_PAD_Y;
    LOG("[mod touch-ui] leave button %p built from the preparation panel's back button %p under %p: %.0fx%.0f in a %.0fx%.0f panel, selectable %p disabled", clone, source, parent, w, h, parent_rect.w, parent_rect.h, selectable);
}

static void leave_visible_run(uint64_t *r, int p) {
    (void)r; (void)p;
    leave_set_active(s_leave, s_leave_want);
    s_leave_shown = s_leave_want;
    static int n_log;
    if (n_log++ < 20) LOG("[mod touch-ui] leave button %s", s_leave_want ? "shown over the bookshelf" : "hidden");
}

static void leave_exit_run(uint64_t *r, int p) {
    (void)r; (void)p;
    FN(K_INV_SET_PHASE_STATE, void, void *, int, int, void *)(s_leave_panel, PHASE_STORY, 0, NULL);
    LOG("[mod touch-ui] leave button tapped: invitation panel %p leaves the bookshelf", s_leave_panel);
}

static int leave_hit(const dm_mouse_frame_t *f) {
    float cy = 1.0f - LEAVE_Y_FRAC;
    return fabsf(f->nx - LEAVE_X_FRAC) <= s_leave_half_w && fabsf(f->ny - cy) <= s_leave_half_h;
}

static void on_inv_update(uint64_t *r, int p) {
    if (!mod_enabled() || !dm_touchscreen_mode() || !main_base() || !build_matches()) return;
    void *panel = (void *)(uintptr_t)r[0];
    if (!panel) return;
    if (s_leave && s_leave_owner != panel) {
        s_leave = NULL;
        s_leave_owner = NULL;
        s_leave_shown = 0;
        s_leave_pressed = 0;
        s_leave_failed = 0;
    }
    int bookshelf = *(volatile int *)((uintptr_t)panel + F_INV_PHASE) == PHASE_DROP_BOOK_SELECT;
    s_leave_panel = panel;
    if (!s_leave) {
        if (!bookshelf || s_leave_failed) return;
        dm_mod_gate_run(r, p, leave_create_run);
        if (!s_leave) return;
    }
    if (bookshelf != s_leave_shown) {
        s_leave_want = bookshelf;
        dm_mod_gate_run(r, p, leave_visible_run);
        s_leave_pressed = 0;
    }
    if (!bookshelf) return;
    dm_mouse_frame_t f = frame_now();
    int inside = leave_hit(&f);
    if (f.down & DM_MOUSE_LEFT) s_leave_pressed = inside;
    if (f.up & DM_MOUSE_LEFT) {
        int tapped = s_leave_pressed && inside;
        s_leave_pressed = 0;
        if (tapped) dm_mod_gate_run(r, p, leave_exit_run);
    }
}

static void on_icall_resolved(uint64_t *r, int p) {
    if (p == 16) { on_inv_update(r, p); return; }
    if (p == 1) { on_page_press(r, p); return; }
    if (p == 2) { on_cam_late(r, p); return; }
    if (p == 14) { on_hand_update(r); return; }
    if (p == 15) { on_dice_submit(); return; }
    if (p > 2) { on_scroll_ui(r, p - 2); return; }
    if (!mod_enabled()) return;
    const char *name = (const char *)(uintptr_t)r[19];
    if (!name || !r[0]) return;
    if (!strncmp(name, CURSOR_PREFIX, sizeof CURSOR_PREFIX - 1)) {
        if (dm_touchscreen_mode() && pointer_touch()) on_cursor_icall(name, r);
        return;
    }
    if (strncmp(name, INPUT_PREFIX, sizeof INPUT_PREFIX - 1)) return;
    if (!dm_touchscreen_mode()) return;
    const char *method = name + sizeof INPUT_PREFIX - 1;
    for (size_t i = 0; i < sizeof k_icalls / sizeof k_icalls[0]; i++) {
        size_t n = strlen(k_icalls[i].name);
        if (strncmp(method, k_icalls[i].name, n) || (method[n] != '\0' && method[n] != '(')) continue;
        LOG("[mod touch-ui] %s: icall %#llx replaced by the host's touchpad mouse", name, (unsigned long long)r[0]);
        if (k_icalls[i].real) *k_icalls[i].real = (void *)(uintptr_t)r[0];
        r[0] = (uint64_t)(uintptr_t)k_icalls[i].fn;
        return;
    }
}

__attribute__((constructor)) static void mod_touch_ui_attach(void) {
    gate_obs_set(MODGATE_TOUCH_UI_ICALL_RESOLVED, on_icall_resolved);
}
