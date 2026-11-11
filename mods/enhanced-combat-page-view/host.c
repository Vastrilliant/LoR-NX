// Mod "enhanced-combat-page-view": Cyaminthe's Enhanced Combat Page View (Steam Workshop 2802216129), as a host callback on an observation gate.
// The PC mod scales BattleDiceCardUI.vibeRect (a child RectTransform) of the card under the cursor between 1.0 and 1.75, and moves it by sizeDelta * (scale - 1) / 2 so the page grows away from its anchor.
// Here every page in the hand gets the same scale (DM_MOD_ECPV_ZOOM, a multiplier, 1.00 to 1.75, default 1.25: 25% bigger; a value of 10 or more is read as the old percentage, 125 = x1.25), selected or not, so there is no selection state to follow and nothing to undo:
//   update_pre   BattleUnitCardsInHandUI.Update entry (once a frame, x0 = the hand): vibeRect of every page in _cardList gets the scale and the offset (a page that is vibrating is skipped)
//   swap_press   BattleDiceCardUI.OnPdSubmit entry + 4 (x0 = the pressed page, x1 = the event data), Page Swapping (DM_MOD_ECPV_SWAP, Touchscreen Mode only): with a page selected, a left press on it deselects it the way the hand's right click does (SetToDefault, ReleaseSelectedCard, CloseUnitInformation, UnblockOtherUnitsDice); a left press on another page that GetClickableState would accept deselects the selected page and runs ShowDetail on the pressed one, so the game's own OnPdSubmit then selects it
// Not ported: hands of more than 10 pages (the PC mod adds card UI objects), the 65 to 60 rotation constant, and the mouse wheel.
// Off by default: DM_MOD_ECPV=1 turns the callback on (the launcher's Mods > Enhanced Combat Page View switch, or env.txt). The gate is always in the code and passes through while it is off.
#include "dm.h"
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
#define K_SINGLETON_BM 0x2D5C5E0u
#define K_SINGLETON_STAGE 0x2D5CA30u
#define K_INFO_RELEASE 0x21C8C60u
#define K_INFO_CLOSE 0x21C86A0u
#define S_MI_BM 0x51D93E8u
#define S_MI_STAGE 0x51C6C60u

#define F_HAND_SELECTED 0x60
#define F_CARD_ENTERED 0x133
#define F_EVENT_BUTTON 0x144
#define F_STAGE_MODE 0xE0
#define F_BM_INFO_A 0x18
#define F_BM_INFO_B 0x20
#define INPUT_PAD 4
#define CLICK_OK 1

#define F_CARD_VIBERECT 0x18                    // BattleDiceCardUI.vibeRect
#define F_CARD_VIBING 0x1F1                     // BattleDiceCardUI.isRunningVibeCard
#define F_HAND_CARDLIST 0x18                    // BattleUnitCardsInHandUI._cardList (List<BattleDiceCardUI>)
#define F_LIST_ITEMS 0x10
#define F_LIST_SIZE 0x18
#define F_ARRAY_DATA 0x20

typedef struct { float x, y; } v2;
typedef struct { float x, y, z; } v3;

static uint8_t *s_main;
static int g_logged;
static void *s_hand;

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

static void update_run(uint64_t *r, int param) {
    (void)param;
    if (!main_base()) return;
    s_hand = (void *)r[0];
    void *list = field((void *)r[0], F_HAND_CARDLIST);
    void *items = field(list, F_LIST_ITEMS);
    int n = list ? *(const int *)((uintptr_t)list + F_LIST_SIZE) : 0;
    for (int i = 0; i < n && i < 32; i++) {
        void *card = *(void *const *)((uintptr_t)items + F_ARRAY_DATA + sizeof(void *) * (size_t)i);
        if (!card || *(const uint8_t *)((uintptr_t)card + F_CARD_VIBING)) continue;
        set_zoom(card, zoom());
    }
    if (g_logged++ < 3) LOG("[mod enhanced-combat-page-view] hand %#llx: %d page(s)", (unsigned long long)r[0], n);
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
    if (!main_base() || !swap_on()) return;
    void *hand = s_hand, *card = (void *)r[0], *ev = (void *)r[1];
    if (!hand || !card || !ev) return;
    void *selected = field(hand, F_HAND_SELECTED);
    if (!selected || !card_in_hand(hand, card)) return;
    if (FN(K_INPUT_TYPE, int, void *, const void *)(ev, NULL) == INPUT_PAD) return;
    if (*(const int *)((uintptr_t)ev + F_EVENT_BUTTON) != 0) return;
    if (card == selected) {
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

__attribute__((constructor)) static void mod_enhanced_combat_page_view_attach(void) {
    gate_obs_set(MODGATE_ENHANCED_COMBAT_PAGE_VIEW_UPDATE_PRE, on_update_pre);
    gate_obs_set(MODGATE_ENHANCED_COMBAT_PAGE_VIEW_SWAP_PRESS, on_swap_press);
}
