// Mod "enhanced-combat-page-view": Cyaminthe's Enhanced Combat Page View (Steam Workshop 2802216129), as a host callback on an observation gate.
// The PC mod scales BattleDiceCardUI.vibeRect (a child RectTransform) of the card under the cursor between 1.0 and 1.75, and moves it by sizeDelta * (scale - 1) / 2 so the page grows away from its anchor.
// Here every page in the hand gets the same scale (DM_MOD_ECPV_ZOOM, a multiplier, 1.00 to 1.75, default 1.25: 25% bigger; a value of 10 or more is read as the old percentage, 125 = x1.25), selected or not, so there is no selection state to follow and nothing to undo:
//   update_pre   BattleUnitCardsInHandUI.Update entry (once a frame, x0 = the hand): vibeRect of every page in _cardList gets the scale and the offset (a page that is vibrating is skipped); then the Hold to On-Play hold advances
//   swap_press   BattleDiceCardUI.OnPdSubmit entry + 4 (x0 = the pressed page, x1 = the event data): starts the Hold to On-Play hold on a left press of a page that qualifies; then Page Swapping (DM_MOD_ECPV_SWAP, Touchscreen Mode only): with a page selected, a left press on it deselects it the way the hand's right click does (SetToDefault, ReleaseSelectedCard, CloseUnitInformation, UnblockOtherUnitsDice), except on a page the hold is running on, where the deselect waits for the release and happens only if the hold did not complete; a left press on another page that GetClickableState would accept deselects the selected page and runs ShowDetail on the pressed one, so the game's own OnPdSubmit then selects it
//   click_state  BattleDiceCardUI.OnPdSubmit right after its GetClickableState call (x19 = the pressed page, w0 = the state): when swap_press took the press as a tap on the selected page, w0 becomes CanClick_emotion (0), which OnPdSubmit ignores, so the game's own refusal (PlayVibeCard: the shake and its sound) does not play on top of the deselect
//   apply_pre    BattleUnitCardsInHandUI.ApplySelectedCard entry (x0 = the hand), Suppress On-Play deselect (DM_MOD_ECPV_KEEP): remembers the speed die the hand was opened from when the selected page is an On-Play page (range Instance)
//   apply_post   SpeedDiceUI.OnClickSpeedDice after its BattleUnitInformationUI.ReleaseSelectedCard call: clicks the remembered die again, which reopens the hand the way the player's own click on that die does, after ApplySelectedCard has closed it
//   info_alpha   BattleUnitInformationUI.Update right after its CanvasGroup.set_alpha call (x19 = the panel), Fade info panels (DM_MOD_ECPV_DIM): while the hand holds a selected page, the panel's alpha is cut to 25% (75% transparent) and its CanvasGroup stops blocking raycasts, so taps reach what is behind it; the game writes the alpha again every frame and never touches blocksRaycasts, so the alpha reverts by itself and blocksRaycasts is put back when no page is selected
// Not ported: hands of more than 10 pages (the PC mod adds card UI objects), the 65 to 60 rotation constant, and the mouse wheel.
// Off by default: DM_MOD_ECPV=1 turns the callback on (the launcher's Mods > Enhanced Combat Page View switch, or env.txt). The gate is always in the code and passes through while it is off.
#include "dm.h"
#include "dm_input.h"
#include "hle_mods_gen.h"

void dm_mod_gate_run(uint64_t *regs, int param, void (*fn)(uint64_t *, int));

#define K_RECT_GET_SIZEDELTA 0x19BA760u         // RectTransform.get_sizeDelta
#define K_TRANSFORM_SET_LOCALPOS 0x19BAC70u
#define K_TRANSFORM_SET_LOCALSCALE 0x19BD790u

#define K_HAND_SETTODEFAULT 0x1FDF560u
#define K_HAND_UNBLOCK 0x1FDF840u
#define K_CARD_SHOWDETAIL 0x1FD6D10u
#define K_CARD_CLICKABLE 0x1FD6520u
#define K_INPUT_TYPE 0x1BCEAD0u
#define K_DICE_CLICK 0x233F990u
#define K_SINGLETON_BM 0x2D5C5E0u
#define K_SINGLETON_STAGE 0x2D5CA30u
#define K_INFO_RELEASE 0x21C8C60u
#define K_INFO_CLOSE 0x21C86A0u
#define S_MI_BM 0x51D93E8u
#define S_MI_STAGE 0x51C6C60u

#define K_CANVASGROUP_GET_ALPHA 0x34FAD60u
#define K_CANVASGROUP_SET_ALPHA 0x34FADB0u
#define K_CANVASGROUP_GET_BLOCKS 0x34FAEE0u
#define K_CANVASGROUP_SET_BLOCKS 0x34FAF30u
#define K_TIME_FRAMECOUNT 0x19B9380u

#define K_OBJECT_INSTANTIATE_PARENT 0x19B4E90u
#define K_OBJECT_DESTROY 0x19B5310u
#define K_COMPONENT_GET_TRANSFORM 0x19AE1D0u
#define K_COMPONENT_GET_GAMEOBJECT 0x19AE220u
#define K_IMAGE_SET_SPRITE 0x24D56D0u
#define K_GRAPHIC_SET_COLOR 0x24DE110u
#define K_GRAPHIC_SET_RAYCAST 0x24DE170u
#define K_GRAPHIC_GET_MATERIAL 0x24DFAC0u
#define K_GRAPHIC_SET_MATERIAL 0x24DFB80u
#define K_IMAGE_GET_SPRITE 0x24E55C0u
#define K_IMAGE_SET_TYPE 0x24D5A60u
#define K_IMAGE_SET_FILLMETHOD 0x24E5AF0u
#define K_IMAGE_SET_FILLORIGIN 0x24E5CF0u
#define K_IMAGE_SET_FILLAMOUNT 0x24E5BA0u
#define K_TRANSFORM_GET_PARENT 0x19BC440u
#define K_TRANSFORM_GET_SIBLINGINDEX 0x19BF870u
#define K_TRANSFORM_SET_SIBLINGINDEX 0x19BF7A0u
#define IMAGE_TYPE_FILLED 3
#define FILL_VERTICAL 1
#define FILL_ORIGIN_BOTTOM 0
#define K_INPUT_GET_MOUSEBUTTON 0x3530660u
#define K_INPUT_GET_MOUSEPOSITION 0x3530A20u
#define K_TIME_UNSCALED_DT 0x19B91C0u
#define K_HAND_APPLY 0x1FDEB00u
#define K_CTRL_RESET_CURSOR 0x1E22770u
#define S_CTRL_CLASS 0x51DE940u
#define S_MI_NOTIFY 0x51E57D8u
#define K_SCRIPT_BASE_VIRTUALS 0x1C54490u

#define F_HAND_SELECTED 0x60
#define F_HAND_BEFORE_DICE 0xA0
#define F_CARD_MODEL 0x128
#define F_MODEL_XML 0x18
#define F_XML_SPEC 0x50
#define F_SPEC_RANGE 0x10
#define RANGE_INSTANCE 5
#define F_CARD_ENTERED 0x133
#define F_EVENT_BUTTON 0x144
#define F_STAGE_MODE 0xE0
#define F_BM_INFO_A 0x18
#define F_BM_INFO_B 0x20
#define INPUT_PAD 4
#define CLICK_OK 1

#define F_CARD_VIBERECT 0x18                    // BattleDiceCardUI.vibeRect
#define F_CARD_VIBING 0x1F1                     // BattleDiceCardUI.isRunningVibeCard
#define F_HAND_UNIT 0x50
#define F_CARD_ARTWORK 0x68
#define F_CARD_DODGES 0x48
#define F_MODEL_SCRIPT 0x70
#define F_DICE_SLOT 0x160
#define F_DICE_INFO 0x168
#define F_CLASS_STATICS 0xB8
#define F_NOTIFY_FLAG 0x58
#define F_NOTIFY_CALLBACK 0x40
#define F_CALLBACK_INVOKE 0x18
#define F_CALLBACK_TARGET 0x40
#define F_CALLBACK_METHOD 0x28
#define F_ARRAY_LENGTH 0x18
#define VT_BASE 0x138
#define VT_ENTRY 16
#define VT_ON_USE_INSTANCE 39
#define VT_FIRST_TARGET_VIRTUAL 40
#define VT_TARGET_VIRTUALS 4
#define HOLD_SECONDS 0.75f
#define HOLD_GRACE 0.2f
#define HOLD_MOVE_PIXELS 48.0f
#define HOLD_MAX_STEP 0.1f
#define F_INFO_CANVASGROUP 0xD0
#define DIM_ALPHA 0.25f
#define DIM_PANELS 8
#define DIM_MAX_AGE 1
#define F_HAND_CARDLIST 0x18                    // BattleUnitCardsInHandUI._cardList (List<BattleDiceCardUI>)
#define F_LIST_ITEMS 0x10
#define F_LIST_SIZE 0x18
#define F_ARRAY_DATA 0x20

typedef struct { float x, y; } v2;
typedef struct { float x, y, z; } v3;
typedef struct { float r, g, b, a; } rgba;

static const uint32_t k_target_readers[] = {
    0x1B856F0u, 0x1B85BA0u, 0x1B85180u, 0x1C635C0u, 0x1C63280u, 0x1C63420u, 0x1C630E0u, 0x1C62F40u, 0x1C621C0u, 0x1C62980u,
    0x260C4C0u, 0x1C604F0u, 0x1C85220u, 0x1C85370u, 0x1C85910u, 0x1B59140u, 0x1B594B0u, 0x1B59800u, 0x1B59AF0u,
};

static uint8_t *s_main;
static int g_logged;
static void *s_hand;
static void *s_keep_die;
static void *s_swallow;
static int s_hand_frame = -1000;
static int s_dim_evict;
static struct {
    void *group;
    int blocks;
} s_dim[DIM_PANELS];

static void hold_begin(void *hand, void *card, int pad);
static void hold_tick(void *hand);
static int hold_deferred(void *card);

static uint8_t *main_base(void) {
    if (!s_main)
        for (int i = 0; i < g_nmod; i++)
            if (!strcmp(g_mod[i].name, "main") && !g_mod[i].nro) { s_main = g_mod[i].base; break; }
    return s_main;
}

#define FN(rva, ret, ...) ((ret (*)(__VA_ARGS__))(s_main + (rva)))

static void *field(void *self, uintptr_t off) { return self ? *(void *const *)((uintptr_t)self + off) : NULL; }

static float zoom(void) {
    static float z = 0;
    if (z <= 0) {
        const char *v = getenv("DM_MOD_ECPV_ZOOM");
        double m = v && v[0] ? atof(v) : 1.25;
        if (m >= 10.0) m /= 100.0;   // the old percentage (125): an env.txt written for it still works
        if (m < 1.0) m = 1.0;
        if (m > 1.75) m = 1.75;
        z = (float)m;
        LOG("[mod enhanced-combat-page-view] zoom x%.2f (DM_MOD_ECPV_ZOOM=%s)", m, v ? v : "unset");
    }
    return z;
}

static void set_zoom(void *card, float scale) {
    void *rect = field(card, F_CARD_VIBERECT);
    if (!rect) return;
    v2 size = FN(K_RECT_GET_SIZEDELTA, v2, void *, const void *)(rect, NULL);
    v3 pos = {size.x * (scale - 1.0f) / 2.0f, size.y * (scale - 1.0f) / 2.0f, 0};
    v3 sc = {scale, scale, 1.0f};
    FN(K_TRANSFORM_SET_LOCALSCALE, void, void *, v3, const void *)(rect, sc, NULL);
    FN(K_TRANSFORM_SET_LOCALPOS, void, void *, v3, const void *)(rect, pos, NULL);
}

static int dim_on(void) {
    static int on = -1;
    if (on < 0) {
        const char *v = getenv("DM_MOD_ECPV_DIM");
        on = v && v[0] && atoi(v) != 0;
        LOG("[mod enhanced-combat-page-view] fade info panels %s (DM_MOD_ECPV_DIM=%s)", on ? "on" : "off", v ? v : "unset");
    }
    return on;
}

static int frame_count(void) { return FN(K_TIME_FRAMECOUNT, int, const void *)(NULL); }

static void update_run(uint64_t *r, int param) {
    (void)param;
    if (!main_base()) return;
    s_hand = (void *)r[0];
    if (dim_on()) s_hand_frame = frame_count();
    void *list = field((void *)r[0], F_HAND_CARDLIST);
    void *items = field(list, F_LIST_ITEMS);
    int n = list ? *(const int *)((uintptr_t)list + F_LIST_SIZE) : 0;
    for (int i = 0; i < n && i < 32; i++) {
        void *card = *(void *const *)((uintptr_t)items + F_ARRAY_DATA + sizeof(void *) * (size_t)i);
        if (!card || *(const uint8_t *)((uintptr_t)card + F_CARD_VIBING)) continue;
        set_zoom(card, zoom());
    }
    hold_tick((void *)r[0]);
    if (g_logged++ < 3) LOG("[mod enhanced-combat-page-view] hand %#llx: %d page(s)", (unsigned long long)r[0], n);
}

static int page_selected(void) {
    if (!s_hand || frame_count() - s_hand_frame > DIM_MAX_AGE) return 0;
    return field(s_hand, F_HAND_SELECTED) != NULL;
}

static void info_run(uint64_t *r, int param) {
    (void)param;
    if (!main_base() || !dim_on()) return;
    void *group = field((void *)r[19], F_INFO_CANVASGROUP);
    if (!group) return;
    int slot = -1, spare = -1;
    for (int i = 0; i < DIM_PANELS; i++) {
        if (s_dim[i].group == group) slot = i;
        else if (!s_dim[i].group && spare < 0) spare = i;
    }
    if (page_selected()) {
        if (slot < 0) {
            slot = spare >= 0 ? spare : s_dim_evict++ % DIM_PANELS;
            s_dim[slot].group = group;
            s_dim[slot].blocks = (int)(FN(K_CANVASGROUP_GET_BLOCKS, uint32_t, void *, const void *)(group, NULL) & 1u);
            FN(K_CANVASGROUP_SET_BLOCKS, void, void *, int, const void *)(group, 0, NULL);
            LOG("[mod enhanced-combat-page-view] info panel %#llx faded and click-through", (unsigned long long)(uintptr_t)group);
        }
        float alpha = FN(K_CANVASGROUP_GET_ALPHA, float, void *, const void *)(group, NULL);
        FN(K_CANVASGROUP_SET_ALPHA, void, void *, float, const void *)(group, alpha * DIM_ALPHA, NULL);
    } else if (slot >= 0) {
        FN(K_CANVASGROUP_SET_BLOCKS, void, void *, int, const void *)(group, s_dim[slot].blocks, NULL);
        s_dim[slot].group = NULL;
        LOG("[mod enhanced-combat-page-view] info panel %#llx restored", (unsigned long long)(uintptr_t)group);
    }
}

static int swap_on(void) {
    static int on = -1;
    if (on < 0) {
        const char *t = getenv("DM_TOUCHSCREEN");
        const char *s = getenv("DM_MOD_ECPV_SWAP");
        int touch = t && atoi(t);
        int want = !s || !s[0] || atoi(s) != 0;
        on = touch && want;
        LOG("[mod enhanced-combat-page-view] page swapping %s (DM_TOUCHSCREEN=%s DM_MOD_ECPV_SWAP=%s)", on ? "on" : "off", t ? t : "unset", s ? s : "unset");
    }
    return on;
}

static void *singleton(uint32_t fn, uint32_t slot) {
    void **mi = *(void ***)(s_main + slot);
    return mi && *mi ? FN(fn, void *, void *)(*mi) : NULL;
}

static int card_in_hand(void *hand, void *card) {
    void *list = field(hand, F_HAND_CARDLIST);
    void *items = field(list, F_LIST_ITEMS);
    int n = list ? *(const int *)((uintptr_t)list + F_LIST_SIZE) : 0;
    for (int i = 0; i < n && i < 32; i++)
        if (*(void *const *)((uintptr_t)items + F_ARRAY_DATA + sizeof(void *) * (size_t)i) == card) return 1;
    return 0;
}

static void deselect(void *hand) {
    FN(K_HAND_SETTODEFAULT, void, void *, const void *)(hand, NULL);
    void *bm = singleton(K_SINGLETON_BM, S_MI_BM);
    void *stage = singleton(K_SINGLETON_STAGE, S_MI_STAGE);
    void *info = bm && stage ? field(bm, *(const int *)((uintptr_t)stage + F_STAGE_MODE) ? F_BM_INFO_B : F_BM_INFO_A) : NULL;
    if (info) {
        FN(K_INFO_RELEASE, void, void *, const void *)(info, NULL);
        FN(K_INFO_CLOSE, void, void *, int, int, const void *)(info, 0, -1, NULL);
    }
    FN(K_HAND_UNBLOCK, void, void *, const void *)(hand, NULL);
}

static void swap_run(uint64_t *r, int param) {
    (void)param;
    if (!main_base()) return;
    s_swallow = NULL;
    void *hand = s_hand, *card = (void *)r[0], *ev = (void *)r[1];
    if (!hand || !card || !ev) return;
    if (FN(K_INPUT_TYPE, int, void *, const void *)(ev, NULL) == INPUT_PAD) return;
    if (*(const int *)((uintptr_t)ev + F_EVENT_BUTTON) != 0) return;
    if (!card_in_hand(hand, card)) return;
    hold_begin(hand, card, 0);
    if (!swap_on()) return;
    void *selected = field(hand, F_HAND_SELECTED);
    if (!selected) return;
    if (card == selected) {
        s_swallow = card;
        if (hold_deferred(card)) return;
        deselect(hand);
        LOG("[mod enhanced-combat-page-view] page %#llx deselected by a tap", (unsigned long long)(uintptr_t)card);
        return;
    }
    uint8_t *entered = (uint8_t *)((uintptr_t)card + F_CARD_ENTERED);
    uint8_t was = *entered;
    *entered = 1;
    int state = FN(K_CARD_CLICKABLE, int, void *, const void *)(card, NULL);
    *entered = was;
    if (state != CLICK_OK) return;
    deselect(hand);
    FN(K_CARD_SHOWDETAIL, void, void *, const void *)(card, NULL);
    LOG("[mod enhanced-combat-page-view] page %#llx swapped in for %#llx", (unsigned long long)(uintptr_t)card, (unsigned long long)(uintptr_t)selected);
}

static int keep_on(void) {
    static int on = -1;
    if (on < 0) {
        const char *v = getenv("DM_MOD_ECPV_KEEP");
        on = v && v[0] && atoi(v) != 0;
        LOG("[mod enhanced-combat-page-view] suppress on-play deselect %s (DM_MOD_ECPV_KEEP=%s)", on ? "on" : "off", v ? v : "unset");
    }
    return on;
}

static void apply_pre_run(uint64_t *r, int param) {
    (void)param;
    s_keep_die = NULL;
    if (!main_base() || !keep_on()) return;
    void *hand = (void *)r[0];
    void *model = field(field(hand, F_HAND_SELECTED), F_CARD_MODEL);
    void *spec = field(field(model, F_MODEL_XML), F_XML_SPEC);
    if (!spec || *(const int *)((uintptr_t)spec + F_SPEC_RANGE) != RANGE_INSTANCE) return;
    s_keep_die = field(hand, F_HAND_BEFORE_DICE);
}

static void apply_post_run(uint64_t *r, int param) {
    (void)r; (void)param;
    void *die = s_keep_die;
    s_keep_die = NULL;
    if (!die || !main_base()) return;
    FN(K_DICE_CLICK, void, void *, const void *)(die, NULL);
    LOG("[mod enhanced-combat-page-view] on-play page played, speed die %#llx selected again", (unsigned long long)(uintptr_t)die);
}

static int hold_on(void) {
    static int on = -1;
    if (on < 0) {
        const char *v = getenv("DM_MOD_ECPV_HOLD");
        on = v && v[0] && atoi(v) != 0;
        LOG("[mod enhanced-combat-page-view] hold to on-play %s (DM_MOD_ECPV_HOLD=%s)", on ? "on" : "off", v ? v : "unset");
    }
    return on;
}

static int self_only(void *card) {
    void *model = field(card, F_CARD_MODEL);
    void *spec = field(field(model, F_MODEL_XML), F_XML_SPEC);
    if (!spec || *(const int *)((uintptr_t)spec + F_SPEC_RANGE) != RANGE_INSTANCE) return 0;
    void *script = field(model, F_MODEL_SCRIPT);
    if (!script) return 0;
    const uint8_t *klass = *(const uint8_t *const *)script;
    if (!klass) return 0;
    for (int i = 0; i < VT_TARGET_VIRTUALS; i++) {
        const uint8_t *fn = *(const uint8_t *const *)(klass + VT_BASE + VT_ENTRY * (VT_FIRST_TARGET_VIRTUAL + i));
        if (fn != s_main + K_SCRIPT_BASE_VIRTUALS + 0x10u * (uint32_t)i) return 0;
    }
    const uint8_t *on_use = *(const uint8_t *const *)(klass + VT_BASE + VT_ENTRY * VT_ON_USE_INSTANCE);
    for (size_t i = 0; i < sizeof k_target_readers / sizeof k_target_readers[0]; i++)
        if (on_use == s_main + k_target_readers[i]) return 0;
    return 1;
}

static struct {
    void *card;
    void *img;
    float t;
    int pad;
    int defer;
    v2 pos;
} s_hold;

static void *transform_of(void *obj) { return FN(K_COMPONENT_GET_TRANSFORM, void *, void *, const void *)(obj, NULL); }

static void overlay_make(void *card) {
    void *art = field(card, F_CARD_ARTWORK);
    if (!art || !FN(K_IMAGE_GET_SPRITE, void *, void *, const void *)(art, NULL)) return;
    void *art_tf = transform_of(art);
    void *parent = art_tf ? FN(K_TRANSFORM_GET_PARENT, void *, void *, const void *)(art_tf, NULL) : NULL;
    if (!parent) return;
    int index = FN(K_TRANSFORM_GET_SIBLINGINDEX, int, void *, const void *)(art_tf, NULL);
    void *img = FN(K_OBJECT_INSTANTIATE_PARENT, void *, void *, void *, const void *)(art, parent, NULL);
    if (!img) return;
    void *dodges = field(card, F_CARD_DODGES);
    void *dodge = dodges && *(const int *)((uintptr_t)dodges + F_ARRAY_LENGTH) > 0 ? *(void *const *)((uintptr_t)dodges + F_ARRAY_DATA) : NULL;
    void *mat = dodge ? FN(K_GRAPHIC_GET_MATERIAL, void *, void *, const void *)(dodge, NULL) : NULL;
    rgba c = {0.6f, 0.6f, 0.6f, mat ? 1.0f : 0.45f};
    FN(K_IMAGE_SET_TYPE, void, void *, int, const void *)(img, IMAGE_TYPE_FILLED, NULL);
    FN(K_IMAGE_SET_FILLMETHOD, void, void *, int, const void *)(img, FILL_VERTICAL, NULL);
    FN(K_IMAGE_SET_FILLORIGIN, void, void *, int, const void *)(img, FILL_ORIGIN_BOTTOM, NULL);
    FN(K_IMAGE_SET_FILLAMOUNT, void, void *, float, const void *)(img, 0.0f, NULL);
    FN(K_GRAPHIC_SET_RAYCAST, void, void *, int, const void *)(img, 0, NULL);
    FN(K_GRAPHIC_SET_COLOR, void, void *, rgba, const void *)(img, c, NULL);
    if (mat) FN(K_GRAPHIC_SET_MATERIAL, void, void *, void *, const void *)(img, mat, NULL);
    void *img_tf = transform_of(img);
    if (img_tf) FN(K_TRANSFORM_SET_SIBLINGINDEX, void, void *, int, const void *)(img_tf, index + 1, NULL);
    s_hold.img = img;
}

static void overlay_fill(float progress) {
    if (!s_hold.img) return;
    FN(K_IMAGE_SET_FILLAMOUNT, void, void *, float, const void *)(s_hold.img, progress > 1.0f ? 1.0f : progress, NULL);
}

static void hold_end(void *hand) {
    if (s_hold.img && hand && card_in_hand(hand, s_hold.card)) {
        void *go = FN(K_COMPONENT_GET_GAMEOBJECT, void *, void *, const void *)(s_hold.img, NULL);
        if (go) FN(K_OBJECT_DESTROY, void, void *, const void *)(go, NULL);
    }
    memset(&s_hold, 0, sizeof s_hold);
}

static v2 pointer_pos(void) {
    v3 p = FN(K_INPUT_GET_MOUSEPOSITION, v3, const void *)(NULL);
    v2 r = {p.x, p.y};
    return r;
}

static void hold_begin(void *hand, void *card, int pad) {
    if (!hold_on() || !self_only(card)) return;
    if (s_hold.card) hold_end(hand);
    s_hold.card = card;
    s_hold.pad = pad;
    if (!pad) {
        s_hold.pos = pointer_pos();
        s_hold.defer = swap_on() && field(hand, F_HAND_SELECTED) == card;
    }
    overlay_make(card);
    LOG("[mod enhanced-combat-page-view] hold started on page %#llx (%s)", (unsigned long long)(uintptr_t)card, pad ? "pad" : "pointer");
}

static int hold_deferred(void *card) { return s_hold.card == card && s_hold.defer; }

static void play_self(void *hand, void *card) {
    void *unit = field(hand, F_HAND_UNIT);
    void *die = field(hand, F_HAND_BEFORE_DICE);
    if (!unit || !die || field(hand, F_HAND_SELECTED) != card) return;
    int slot = *(const int *)((uintptr_t)die + F_DICE_SLOT);
    s_keep_die = keep_on() ? die : NULL;
    FN(K_HAND_APPLY, void, void *, void *, int, const void *)(hand, unit, slot, NULL);
    void **cell = *(void ***)(s_main + S_CTRL_CLASS);
    uint8_t *klass = cell ? *(uint8_t **)cell : NULL;
    void **statics = klass ? *(void ***)(klass + F_CLASS_STATICS) : NULL;
    void *ctrl = statics ? statics[0] : NULL;
    if (ctrl) FN(K_CTRL_RESET_CURSOR, void, void *, int, int, const void *)(ctrl, 0, 1, NULL);
    void *info = field(die, F_DICE_INFO);
    if (info) FN(K_INFO_RELEASE, void, void *, const void *)(info, NULL);
    void *notify = singleton(K_SINGLETON_BM, S_MI_NOTIFY);
    if (notify && *(const uint8_t *)((uintptr_t)notify + F_NOTIFY_FLAG)) {
        void *cb = field(notify, F_NOTIFY_CALLBACK);
        if (cb) ((void (*)(void *, void *))*(void **)((uintptr_t)cb + F_CALLBACK_INVOKE))(field(cb, F_CALLBACK_TARGET), field(cb, F_CALLBACK_METHOD));
    }
    LOG("[mod enhanced-combat-page-view] on-play page %#llx played without a target (die %#llx, slot %d)", (unsigned long long)(uintptr_t)card, (unsigned long long)(uintptr_t)die, slot);
    apply_post_run(NULL, 0);
}

static void hold_tick(void *hand) {
    if (!hold_on() || !main_base()) return;
    void *selected = field(hand, F_HAND_SELECTED);
    int x_held = (dm_mod_keys() & DM_BTN_X) != 0;
    if (!s_hold.card && x_held && selected && card_in_hand(hand, selected)) hold_begin(hand, selected, 1);
    void *card = s_hold.card;
    if (!card) return;
    if (!card_in_hand(hand, card)) {
        memset(&s_hold, 0, sizeof s_hold);
        return;
    }
    int held = s_hold.pad ? x_held : (FN(K_INPUT_GET_MOUSEBUTTON, uint32_t, int, const void *)(0, NULL) & 1u) != 0;
    if (!held) {
        int tap = s_hold.defer;
        hold_end(hand);
        if (tap) {
            deselect(hand);
            LOG("[mod enhanced-combat-page-view] page %#llx deselected by a tap", (unsigned long long)(uintptr_t)card);
        }
        return;
    }
    if (!s_hold.pad) {
        v2 p = pointer_pos();
        float dx = p.x - s_hold.pos.x, dy = p.y - s_hold.pos.y;
        if (dx * dx + dy * dy > HOLD_MOVE_PIXELS * HOLD_MOVE_PIXELS) {
            hold_end(hand);
            return;
        }
    }
    if (selected != card && s_hold.t > HOLD_GRACE) {
        hold_end(hand);
        return;
    }
    float dt = FN(K_TIME_UNSCALED_DT, float, const void *)(NULL);
    s_hold.t += dt > HOLD_MAX_STEP ? HOLD_MAX_STEP : dt;
    overlay_fill(s_hold.t / HOLD_SECONDS);
    if (s_hold.t < HOLD_SECONDS || selected != card) return;
    hold_end(hand);
    play_self(hand, card);
}

static int mod_enabled(void) {
    static int on = -1;
    if (on < 0) {
        const char *v = getenv("DM_MOD_ECPV");
        on = v && v[0] == '1';
        LOG("[mod enhanced-combat-page-view] %s (DM_MOD_ECPV=%s)", on ? "on" : "off", v ? v : "unset");
    }
    return on;
}

static void on_update_pre(uint64_t *r, int p) { if (mod_enabled()) dm_mod_gate_run(r, p, update_run); }
static void on_swap_press(uint64_t *r, int p) { if (mod_enabled()) dm_mod_gate_run(r, p, swap_run); }
static void on_apply_pre(uint64_t *r, int p) { if (mod_enabled()) dm_mod_gate_run(r, p, apply_pre_run); }
static void on_apply_post(uint64_t *r, int p) { if (mod_enabled()) dm_mod_gate_run(r, p, apply_post_run); }
static void on_click_state(uint64_t *r, int p) {
    (void)p;
    if (!s_swallow || s_swallow != (void *)r[19]) return;
    s_swallow = NULL;
    r[0] = 0;
}
static void on_info_alpha(uint64_t *r, int p) { if (mod_enabled()) dm_mod_gate_run(r, p, info_run); }

__attribute__((constructor)) static void mod_enhanced_combat_page_view_attach(void) {
    gate_obs_set(MODGATE_ENHANCED_COMBAT_PAGE_VIEW_UPDATE_PRE, on_update_pre);
    gate_obs_set(MODGATE_ENHANCED_COMBAT_PAGE_VIEW_SWAP_PRESS, on_swap_press);
    gate_obs_set(MODGATE_ENHANCED_COMBAT_PAGE_VIEW_APPLY_PRE, on_apply_pre);
    gate_obs_set(MODGATE_ENHANCED_COMBAT_PAGE_VIEW_APPLY_POST, on_apply_post);
    gate_obs_set(MODGATE_ENHANCED_COMBAT_PAGE_VIEW_INFO_ALPHA, on_info_alpha);
    gate_obs_set(MODGATE_ENHANCED_COMBAT_PAGE_VIEW_CLICK_STATE, on_click_state);
}
