// Mods (shared by every mod; built always, by the host Makefile wildcard): run a gate callback on the guest's own stack, so that it can call the game's functions.
//
// An observation gate's callback runs on the thread's gate stack (entry.S dm_gate), and that stack was built for handlers that never run guest code:
//   - guest code issues SVCs and timer reads, which enter dm_gate again, take the same gate-stack top and overwrite the outer frame (dm_gate_nested stops it only when sp is in the window);
//   - Boehm's collector (IL2CPP) scans the thread from its sp up to its stack base, and an sp on the gate stack is outside that range;
//   - a managed pointer that only the host's frames hold would be on the gate stack, where nobody scans.
// dm_mod_gate_run fixes all three for the duration of one callback:
//   1. the thread's gate top (dm_gate_top[k]) is moved below the current host frame, so a nested gate builds its frame under the outer one and the outer frame survives;
//   2. fn runs on the guest stack, below the gate block's push and 512 bytes of margin for the guest's red zone, as if it were a guest function called from the gated instruction;
//      a pause request (on_pause_sig in hle_thread.c) then sees a pc in a module, an sp on the guest stack, and everything fn holds in frames the collector scans;
//   3. x0..x28 of the gate frame are copied into a frame on the guest stack and fn receives that copy, so the pointers the gated code held in registers are roots too.
// The copy is read-only: a callback that must change a register of the gated code cannot use this and runs on the gate stack as before.
// A callback that fires while another is running on the same thread (a game function the callback calls hits a gate of the same mod) returns at once.
#include "dm.h"

extern uint64_t dm_gate_top[];   // entry.S, one per gate slot
#define GATE_SLOTS 1024          // = thread.c GATE_SLOTS

void dm_modcall_on_stack(void (*fn)(void *), void *arg, void *sp);
__asm__(
    ".text\n"
    ".p2align 2\n"
    ".globl _dm_modcall_on_stack\n"
    "_dm_modcall_on_stack:\n"
    "    stp x29, x30, [sp, #-16]!\n"
    "    mov x29, sp\n"            // the frame record points at the caller's, so the crash backtrace walks on through
    "    mov x16, x0\n"
    "    mov x0, x1\n"
    "    mov sp, x2\n"
    "    blr x16\n"
    "    mov sp, x29\n"
    "    ldp x29, x30, [sp], #16\n"
    "    ret\n");

typedef struct { void (*fn)(uint64_t *, int); const uint64_t *regs; int param; } run_t;

static void run_on_guest_stack(void *p) {
    run_t *r = p;
    uint64_t roots[30];
    for (int i = 0; i < 29; i++) roots[i] = r->regs[i];
    roots[29] = 0;
    __asm__ volatile("" : : "r"(roots) : "memory");   // the copy is really stored, in this frame
    r->fn(roots, r->param);
    __asm__ volatile("" : : "r"(roots) : "memory");   // and still there while fn ran
}

static __thread int g_in_mod;

int (*dm_press_owner)(void *card, void *event);

void dm_mod_gate_run(uint64_t *regs, int param, void (*fn)(uint64_t *, int)) {
    if (g_in_mod) return;
    int k = -1;
    const uint64_t top = (uint64_t)(uintptr_t)regs + 800;   // dm_gate_top[k] of the thread whose gate this is: the frame is the 800 bytes below it
    for (int i = 0; i < GATE_SLOTS; i++) if (dm_gate_top[i] == top) { k = i; break; }
    if (k < 0) { static int once; if (!once++) LOG("[mod] gate frame %p is on no gate stack: callback skipped", (void *)regs); return; }
    uint64_t sp_now; __asm__ volatile("mov %0, sp" : "=r"(sp_now));
    const uint64_t guest_sp = (regs[99] - 512) & ~15ull;    // regs[99]: the gate block's sp (entry.S +792), just below the guest's own
    const uint64_t new_top = (sp_now - 512) & ~15ull;        // nested gates build below this function's frame
    run_t r = { fn, regs, param };
    g_in_mod = 1;
    dm_gate_top[k] = new_top;
    dm_modcall_on_stack(run_on_guest_stack, &r, (void *)(uintptr_t)guest_sp);
    dm_gate_top[k] = top;
    g_in_mod = 0;
}
