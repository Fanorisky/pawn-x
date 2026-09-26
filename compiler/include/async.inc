#if defined _inc_async
    #endinput
#endif
#define _inc_async

/* async.inc -- native async/await runtime library (pawn-x experiment 012).
 *
 * pawn on SA-MP/open.mp is single-threaded, so async/await here is NOT
 * parallelism: it is a coroutine transform. An "async" function suspends at each
 * "await", control returns to its caller, and when the awaited operation
 * completes the function resumes exactly where it left off with the result bound.
 * The compiler emits the state machine; this file is the small in-script
 * scheduler that owns each coroutine's state block and drives its resume.
 *
 * ============================ PUBLIC API =================================
 *
 *   Async_Start(Fn, args...) -> token
 *       Start "async" function Fn: allocate its state block, run the body to its
 *       first "await", and return a token naming the parked coroutine (0 if the
 *       arena is full). "Fn" must name an "async" function.
 *
 *   Async_Resume(token, value)
 *       Deliver "value" to the coroutine parked under "token" and resume it; the
 *       value becomes the result of the "await" it was suspended on. When the
 *       coroutine completes, its slot is freed automatically. Resuming an
 *       unknown/already-completed token is a safe no-op. THIS is the entry a real
 *       host adapter (timer/dialog/DB/HTTP completion) would call -- writing such
 *       an adapter is out of MVP scope; tests drive it from a synthetic pump.
 *
 *   Async_ActiveCount() -> count
 *       Number of coroutines currently allocated (parked or running). Baseline 0;
 *       returns to 0 after every started coroutine completes (lifetime contract).
 *
 *   Async_Pending() -> token          (use as: new v = await Async_Pending();)
 *       LEAF AWAITABLE. Called inside an "async" body, it self-registers the
 *       RUNNING coroutine with the scheduler and evaluates to its token, so an
 *       external event source can later resume it via Async_Resume(token, value).
 *       Use it when the coroutine was started as an inner of a composition (its
 *       Async_Start token belongs to the chain); a plain top-level coroutine can
 *       instead "await 0" and be resumed by the token Async_Start returned.
 *
 *   Async_ResumeInner(value)
 *       Pump helper: resume the most-recently self-parked coroutine (the last
 *       Async_Pending()) with "value". A convenience for single-pending
 *       demos/tests; a real event source keeps its own token and calls
 *       Async_Resume(token, value) directly.
 *
 * ============================ WRITING ASYNC CODE =========================
 *
 *   async GetScore(playerid)              // mark the function "async"
 *   {
 *       new base = 100;                   // scalar locals survive an await
 *       new tag[8] = "hi";                // ...and so do array/string locals
 *       new s = await AddScore(playerid, base);   // suspend; resume w/ result
 *       printf("%s score=%d\n", tag, s + base);   // 'base'/'tag' still intact
 *       return s + base;                  // delivers to this fn's awaiter
 *   }
 *
 *   new t = Async_Start(GetScore, 7);     // runs to the await, returns here
 *   Async_Resume(t, 107);                 // a pump/host completion resumes it
 *
 *   "await asyncFn(args)" composes: it starts asyncFn and parks the caller until
 *   asyncFn's "return" delivers its value straight back (return-to-awaiter).
 *
 * ============================ SUPPORTED ACROSS AN AWAIT ==================
 *   * Scalar locals.
 *   * 1-D AND multi-dimensional array/string LOCALS. Their cells (and, for a
 *     multi-dim array, the indirection vector) are LIFTED into the coroutine's
 *     own state block, so they live across the suspend with no save/restore and
 *     read back intact on resume. (This lifts the old v1 "scalar locals only /
 *     error 096" restriction.)
 *   * The per-coroutine block comes from SIZE-CLASSED ARENAS -- default 8 / 32 /
 *     128 / 512 cells (ASYNC_C0..C3_CELLS). A coroutine is served from the
 *     smallest class whose block fits; one whose block exceeds the LARGEST class
 *     cleanly NO-STARTs -- Async_Start returns token 0 (invalid) -- rather than
 *     corrupting storage. Every class cell-count AND slot-count is overridable
 *     with a #define (ASYNC_C0..C3_CELLS / ASYNC_C0..C3_MAX) before this include.
 *   * NOTE: each arena class SLOT COUNT (ASYNC_cK_MAX) must be >= 2. The per-class
 *     stride is derived from one-past-slot-0 (__addressof(g_arenaK[ASYNC_cK_CELLS])),
 *     so a class configured with a single slot would index one element out of
 *     bounds -- keep every ASYNC_C0..C3_MAX at 2 or more.
 *
 * ============================ COMBINATORS & FAULTS ======================
 *   * Async_All(n) / Async_Any(n) + "await Async_Wait(g)" fan several outstanding
 *     operations into one awaiter (task_all / task_any parity); Async_GateFeed(g,
 *     value) reports each completion. See the COMBINATORS block below.
 *   * Async_ResumeError(token, err) / Async_GateFail(gate, err) report FAILURE; the
 *     coroutine checks Async_Failed()/Async_Error() right after the await
 *     (task_set_error parity, leaf-await model). See the fault-channel block below.
 *   * Async_Fail(err) inside an async body, followed by "return", AUTO-RAISES err
 *     into the awaiter of a composed "await asyncFn()" -- fault propagation up the
 *     compose chain, no explicit re-signal at each level. See the fault-channel block.
 *   * MID-EXPRESSION await -- "await" may sit inside a larger expression with live
 *     operand temporaries ("p + await F()", "base + await asyncFn()", ...). The live
 *     operand-stack region is spilled into B before the suspend and restored after
 *     (runtime-exact copy), covering both LEAF and COMPOSED awaits mid-expression.
 *   * MULTIPLE awaits in ONE statement -- "await" binds at unary precedence, so
 *     "await A() + await B()" is "(await A()) + (await B())": two independent
 *     suspends the enclosing operator combines. Works for leaf+leaf, three-in-a-row,
 *     an await in a ternary/operator, composed+composed, and leaf-then-composed.
 *   * A leaf "await" as a call ARGUMENT, in a FIXED-ARITY or VARIADIC call
 *     ("foo(await F(), p, q)", "printf(\"%d\", await F())"), in any argument position,
 *     including nested calls. The sibling arguments (pushed before it at run time)
 *     are spilled and restored across the suspend.
 *   * COMPOSED "await asyncFn()" inside a while/for/do loop ("for (...) total +=
 *     await Step(i);") -- each iteration starts the inner from the arena and frees
 *     it on completion; the operand stack balances across the back-edge. Works with
 *     break/continue and past ASYNC_MAX iterations (arena reuse).
 *   * FIXED-SIZE 1-D array / string PARAMETERS (foo(buf[4]), Greet(name[16])): the
 *     prologue COPIES the caller's cells into the coroutine's block on the fresh
 *     call, so the coroutine owns them and they survive the suspend. Works for a
 *     top-level Async_Start arg and a composed "await F(arr)" arg.
 *   * MULTI-RESULT delivery: Async_ResumeArr(token, values, size) hands a coroutine
 *     an ARRAY (the await returns the element count); the body reads it with
 *     Async_InboxArr(dest) (await_arr parity). See the MULTI-RESULT block below.
 *
 * ============================ STILL UNSUPPORTED =========================
 *   * UNSIZED ("buf[]"), MULTI-DIMENSIONAL, or by-"&"reference PARAMS across an
 *     await -- rejected with error 268: no compile-time cell count to copy in, or
 *     the parameter aliases the caller's frame (which is gone at the suspend). Give
 *     an array parameter a fixed size ("buf[64]") so it can be copied in, or read it
 *     before the first await.
 *   * "await" inside a "foreach" -- rejected (error 099); "foreach" over an async
 *     fn -- rejected (error 267).
 *   * A COMPOSED await whose result a following LEAF await must carry across its own
 *     suspend, in one expression ("await Inner() + await X()") -- rejected (error
 *     099): the composed await returns through the awaiter call.pri chain and a
 *     following leaf suspend cannot unwind back to a resumable point. Reorder so the
 *     leaf comes first, or split into separate statements. (Every other multi-await
 *     shape works -- see SUPPORTED.)
 *   * A suspend at arbitrary call-stack depth inside a non-async helper is
 *     unsupported (needs whole-frame handling).
 *   * IMPLICIT fault auto-raise: a leaf fault an inner IGNORED does not raise by
 *     itself -- the inner must call Async_Fail(err) to propagate (explicit raise IS
 *     supported and shipped; see below). A normally-returning inner (incl. one that
 *     RECOVERED from a leaf fault) hands its awaiter a clean channel.
 *   * Single compilation unit (one .amx).
 *
 * ============================ NON-GOALS (MVP) ============================
 *   * A real host adapter now SHIPS (async_omp.inc: Async_Ms/Async_Ticks over
 *     open.mp SetTimerEx; callback-await via a public that calls Async_Resume).
 *     Async_Resume / Async_GateFeed remain the seams for other event sources
 *     (dialog/DB/HTTP completions).
 *   * JIT compatibility is unverified (same open question as "yield").
 *   * Multi-result "await X() -> (a, b)" binding -- a single scalar result only.
 *   * Cross-.amx await.
 *
 * ============================ INTERNALS ==================================
 * Everything below whose name starts with "__async_" (the compiler intrinsics
 * __async_start / __async_resume / __async_self) or that is documented "internal"
 * (Async_Alloc, Async_Register, Async_Free, Async_FreeByAddr, __async_tok, the
 * g_async* tables) is machinery used by the compiler-emitted code and the public
 * wrappers above -- not part of the surface user code should call directly.
 *
 * Lifetime & safety (task 3): a coroutine's B outlives the frame that started it,
 * and coroutines complete in ARBITRARY order. The AMX heap is LIFO (a single HEA
 * cursor moved by "modheap"/"heap"), so an arbitrary non-top block cannot be
 * released by lowering HEA without corrupting the heap. B blocks therefore come
 * from a FIXED, SIZE-CLASSED ARENA (g_arena0..g_arena3) in the data segment: a
 * block is a slot, the TOKEN is its 1-based global index, Async_Alloc() hands out
 * a free slot (right-sized to the coroutine's block) as a stable address, and
 * Async_Free() returns it to the pool (double-free is a no-op), so starting+
 * completing N coroutines reuses slots and never grows storage.
 */

#include <core>                 /* heapspace() -- used by the lifetime test */

#if !defined ASYNC_MAX
    #define ASYNC_MAX 16            /* count of the SMALL (class-0) coroutine slots */
#endif

/* SIZE-CLASSED ARENA (task 1: variable per-coroutine block size).
 *
 * The old arena was a single uniform stride -- every slot was ASYNC_BLOCK_CELLS
 * (64) cells, so a coroutine with "new buf[100]" overflowed its block and
 * Async_Alloc cleanly refused to start it. Right-sizing every slot to the max
 * would balloon the static footprint (ASYNC_MAX * max_cells). Instead the arena
 * is split into SIZE CLASSES: several small blocks, fewer large ones. A
 * coroutine's block is served from the SMALLEST class whose cell count is >= its
 * required blkcells (= gen_reserved(4) + lifted params/locals). Only when even
 * the LARGEST class cannot hold the block does Async_Alloc return the clean -1
 * no-start sentinel.
 *
 * Each class k is a flat 1-D array g_arenaK of (ASYNC_cK_MAX blocks * ASYNC_cK_CELLS
 * cells). Slots are numbered in one contiguous space across classes -- class 0
 * takes slots [0, C0), class 1 [C0, C0+C1), etc. -- so the TOKEN is still a single
 * 1-based slot index and the g_asyncB/Alive/Running tables stay flat. A slot's
 * block base and, in reverse, the slot owning a given block address are both
 * derived from the per-class base+stride (via __addressof, so cell-size portable),
 * which is what lets Async_FreeByAddr recover the right token when blocks differ
 * in size. */
#if !defined ASYNC_C0_CELLS
    #define ASYNC_C0_CELLS 8            /* class 0: small scalar coroutines */
#endif
#if !defined ASYNC_C1_CELLS
    #define ASYNC_C1_CELLS 32           /* class 1: a few locals / short strings */
#endif
#if !defined ASYNC_C2_CELLS
    #define ASYNC_C2_CELLS 128          /* class 2: mid arrays (e.g. new buf[100]) */
#endif
#if !defined ASYNC_C3_CELLS
    #define ASYNC_C3_CELLS 512          /* class 3: large arrays (the largest class) */
#endif

#if !defined ASYNC_C0_MAX
    #define ASYNC_C0_MAX ASYNC_MAX      /* keep the historical small-coroutine count */
#endif
#if !defined ASYNC_C1_MAX
    #define ASYNC_C1_MAX 8
#endif
#if !defined ASYNC_C2_MAX
    #define ASYNC_C2_MAX 4
#endif
#if !defined ASYNC_C3_MAX
    #define ASYNC_C3_MAX 2
#endif

/* Total slots (flat token space) and the largest servable block. */
#define ASYNC_SLOTS         (ASYNC_C0_MAX + ASYNC_C1_MAX + ASYNC_C2_MAX + ASYNC_C3_MAX)
#define ASYNC_LARGEST_CELLS (ASYNC_C3_CELLS)

/* First global slot index of each class (class 0 starts at 0). */
#define ASYNC_C1_BASE (ASYNC_C0_MAX)
#define ASYNC_C2_BASE (ASYNC_C0_MAX + ASYNC_C1_MAX)
#define ASYNC_C3_BASE (ASYNC_C0_MAX + ASYNC_C1_MAX + ASYNC_C2_MAX)

/* The four class arenas, each a flat 1-D array (no indirection vector). */
static stock g_arena0[ASYNC_C0_MAX * ASYNC_C0_CELLS];
static stock g_arena1[ASYNC_C1_MAX * ASYNC_C1_CELLS];
static stock g_arena2[ASYNC_C2_MAX * ASYNC_C2_CELLS];
static stock g_arena3[ASYNC_C3_MAX * ASYNC_C3_CELLS];

static stock g_asyncB[ASYNC_SLOTS];       /* slot -> state block B (0 = empty) */
static stock g_asyncAlive[ASYNC_SLOTS];   /* slot -> 1 while the coroutine is allocated/parked */
static stock g_asyncRunning[ASYNC_SLOTS]; /* slot -> 1 while its body is ON THE STACK (reentrancy guard) */

/* Slot chosen by the most recent Async_Alloc, so Async_Start can turn the
 * returned block address back into its token. -1 means the arena was full. */
stock g_asyncAllocSlot = -1;

/* async_slot_base(slot): the STABLE data address of the block owned by global
 * slot index "slot", derived from its class's base + stride (via __addressof, so
 * cell-size portable). Inverse of async_slot_of_addr. */
stock async_slot_base(slot)
{
    if (slot < ASYNC_C1_BASE)           /* class 0 */
    {
        new b = __addressof(g_arena0[0]);
        return b + slot * (__addressof(g_arena0[ASYNC_C0_CELLS]) - b);
    }
    if (slot < ASYNC_C2_BASE)           /* class 1 */
    {
        new b = __addressof(g_arena1[0]);
        return b + (slot - ASYNC_C1_BASE) * (__addressof(g_arena1[ASYNC_C1_CELLS]) - b);
    }
    if (slot < ASYNC_C3_BASE)           /* class 2 */
    {
        new b = __addressof(g_arena2[0]);
        return b + (slot - ASYNC_C2_BASE) * (__addressof(g_arena2[ASYNC_C2_CELLS]) - b);
    }
    new b = __addressof(g_arena3[0]);   /* class 3 (largest) */
    return b + (slot - ASYNC_C3_BASE) * (__addressof(g_arena3[ASYNC_C3_CELLS]) - b);
}

/* async_slot_of_addr(B): the global slot index whose block base is exactly the
 * address B, or -1 if B is not an arena block base. Each class is a separate
 * array, so B is tested against each class's [base, base+count*stride) range and
 * required to be slot-aligned; a non-arena address matches nothing. This is how
 * Async_FreeByAddr recovers the PHYSICAL slot for a block regardless of its size
 * class -- and it resolves to the arena slot even when a self-parked coroutine
 * has ALSO aliased the same B under a second (Async_Register) slot. */
stock async_slot_of_addr(B)
{
    new base, stride, off, idx;

    base   = __addressof(g_arena0[0]);
    stride = __addressof(g_arena0[ASYNC_C0_CELLS]) - base;
    off = B - base;
    if (off >= 0) { idx = off / stride; if (idx < ASYNC_C0_MAX && off - idx*stride == 0) return idx; }

    base   = __addressof(g_arena1[0]);
    stride = __addressof(g_arena1[ASYNC_C1_CELLS]) - base;
    off = B - base;
    if (off >= 0) { idx = off / stride; if (idx < ASYNC_C1_MAX && off - idx*stride == 0) return ASYNC_C1_BASE + idx; }

    base   = __addressof(g_arena2[0]);
    stride = __addressof(g_arena2[ASYNC_C2_CELLS]) - base;
    off = B - base;
    if (off >= 0) { idx = off / stride; if (idx < ASYNC_C2_MAX && off - idx*stride == 0) return ASYNC_C2_BASE + idx; }

    base   = __addressof(g_arena3[0]);
    stride = __addressof(g_arena3[ASYNC_C3_CELLS]) - base;
    off = B - base;
    if (off >= 0) { idx = off / stride; if (idx < ASYNC_C3_MAX && off - idx*stride == 0) return ASYNC_C3_BASE + idx; }

    return -1;                          /* not an arena block base */
}

/* Async_Alloc(): claim a free arena slot sized for "blkcells" and return the
 * STABLE data address of its block (which the compiler uses as B). Records the
 * chosen slot in g_asyncAllocSlot so Async_Start can recover the token. Returns
 * -1 (and sets g_asyncAllocSlot = -1) when the block is too large for even the
 * largest size class, OR when every class that could hold it is full -- the
 * compiler guards on this and starts no coroutine in that case. NOTE the sentinel
 * is -1, not 0: a block can legitimately sit at data address 0 (an arena may be
 * the first global), so 0 is a VALID block address and must never mean "invalid".
 * "blkcells" is the compiler-known size of this coroutine's block (gen_reserved +
 * lifted params/locals). A block is served from the SMALLEST class that fits; the
 * scan starts at that class's first slot and continues upward through every
 * larger class, so a full small class transparently falls up to a bigger one.
 * Strides come from the arenas themselves (__addressof differences), cell-size
 * portable. */
stock Async_Alloc(blkcells)
{
    if (blkcells > ASYNC_LARGEST_CELLS)
    {
        g_asyncAllocSlot = -1;
        return -1;                      /* too large for even the largest class: clean sentinel */
    }
    /* first slot of the smallest class whose cell count fits blkcells */
    new lo;
    if (blkcells <= ASYNC_C0_CELLS)      lo = 0;
    else if (blkcells <= ASYNC_C1_CELLS) lo = ASYNC_C1_BASE;
    else if (blkcells <= ASYNC_C2_CELLS) lo = ASYNC_C2_BASE;
    else                                 lo = ASYNC_C3_BASE;

    for (new i = lo; i < ASYNC_SLOTS; i++)   /* fitting class, then every larger class */
    {
        if (!g_asyncAlive[i])
        {
            new B = async_slot_base(i);
            g_asyncAlive[i]   = 1;
            g_asyncRunning[i] = 0;
            g_asyncB[i]       = B;
            g_asyncAllocSlot  = i;
            return B;
        }
    }
    g_asyncAllocSlot = -1;
    return -1;                          /* no fitting slot free (arena full) */
}

/* __async_tok(B): turn the block returned by __async_start into a token. B==-1
 * means the arena was full (no coroutine started) -> token 0 (invalid). */
stock __async_tok(B)
{
    if (B == -1)
        return 0;
    return g_asyncAllocSlot + 1;        /* token: 1-based so 0 is always invalid */
}

/* Async_Start(Fn, args...): allocate an arena block, start coroutine Fn (runs to
 * its first "await"), and return its token. Fn must name an "async" function.
 * Implemented as a macro so the start intrinsic keeps the callee named at the
 * call site (and so __async_start is emitted in the caller's own frame). */
#define Async_Start(%1) __async_tok(__async_start(%1))

/* Async_Register(B): record an EXISTING coroutine block B (e.g. a self-parking
 * coroutine's own B from __async_self()) in a free slot and return its token, or
 * 0 if the table is full. Unlike Async_Alloc it does NOT hand out arena storage;
 * it just references a block that already exists. */
stock Async_Register(B)
{
    for (new i = 0; i < ASYNC_SLOTS; i++)
    {
        if (!g_asyncAlive[i])
        {
            g_asyncB[i]       = B;
            g_asyncAlive[i]   = 1;
            g_asyncRunning[i] = 0;
            return i + 1;               /* token: 1-based */
        }
    }
    return 0;                           /* table full */
}

/* Async_Free(token): return a slot to the free pool. Freeing an out-of-range or
 * already-free slot is a safe NO-OP (double-free guard), so a stale token whose
 * coroutine already completed can never corrupt the arena. */
stock Async_Free(token)
{
    new idx = token - 1;
    if (idx < 0 || idx >= ASYNC_SLOTS)
        return;                         /* out of range: no-op */
    if (!g_asyncAlive[idx])
        return;                         /* already free: double-free no-op */
    g_asyncAlive[idx]   = 0;
    g_asyncRunning[idx] = 0;
    g_asyncB[idx]       = 0;
}

/* Async_Complete(token): a coroutine finished -> free its slot. Kept as a name
 * for the completion path; identical to Async_Free (free-on-complete). */
#define Async_Complete(%1) Async_Free(%1)

/* Async_FreeByAddr(B): free the arena slot whose block base is the address B.
 * The completing coroutine calls this on its OWN B (via the compiler-emitted
 * completion path) so a coroutine that finishes through the nested return-to-
 * awaiter call.pri chain -- and is therefore never observed "done" by a top-level
 * Async_Resume -- still reclaims its slot. The slot is recovered from the address
 * via async_slot_of_addr, which consults each size class's base/stride, so blocks
 * of DIFFERENT sizes are freed correctly. An address that is not an arena block
 * base is a safe no-op, as is a double-free. */
stock Async_FreeByAddr(B)
{
    new idx = async_slot_of_addr(B);
    if (idx < 0)
        return;                         /* not an arena block base: no-op */
    Async_Free(idx + 1);                /* double-free safe */
}

/* Async_ActiveCount(): number of arena slots currently allocated (parked or
 * running). Baseline is 0; after every started coroutine has completed it must
 * return to 0 again -- the binding lifetime contract. */
stock Async_ActiveCount()
{
    new count = 0;
    for (new i = 0; i < ASYNC_SLOTS; i++)
        if (g_asyncAlive[i])
            count++;
    return count;
}

/* Async_Resume(token, value): deliver "value" to the coroutine parked under
 * "token" and resume it. Resuming an out-of-range, never-parked or already
 * completed token is a safe no-op.
 *
 * REENTRANCY GUARD (task 3): while a coroutine's body is on the stack its slot is
 * flagged "running". If a resume arrives for a slot that is already running -- a
 * synchronous / re-entrant completion where the pump ends up calling Async_Resume
 * for the SAME coroutine from within its own body path -- we REJECT it (no-op)
 * instead of re-entering a live frame, which would corrupt its state. When the
 * coroutine completes on this resume its token is freed automatically.
 *
 * FAULT CHANNEL (phase 3, task_set_error parity): a completing operation can
 * report FAILURE instead of a value via Async_ResumeError(token, err). The
 * coroutine observes it right after the await with Async_Failed()/Async_Error()
 * -- the leaf-await error model. (Auto-raise propagation ACROSS a composed
 * "await asyncFn()" chain is the compiler-level follow-on; see the spike note in
 * the header.) g_asyncFailed/g_asyncErr are set for the resume the coroutine is
 * about to observe and are only meaningful in the run immediately following the
 * await, before the coroutine's next await/return -- check them there. */
stock g_asyncFailed = 0;                /* did the await the coroutine just resumed from fail? */
stock g_asyncErr    = 0;                /* error code delivered with that failure */
stock g_asyncPropagate    = 0;          /* an async body called Async_Fail() -> raise into its awaiter */
stock g_asyncPropagateErr = 0;          /* the code to raise */

/* __async_deliver: the shared resume core for Async_Resume / Async_ResumeError.
 * Sets the fault globals the resuming body will observe, then drives one resume
 * under the reentrancy guard and reclaims the slot on completion. Internal. */
stock __async_deliver(token, value, bool:failed, err)
{
    new idx = token - 1;
    if (idx < 0 || idx >= ASYNC_SLOTS)
        return;                         /* unknown token */
    if (!g_asyncAlive[idx])
        return;                         /* never parked / already completed */
    if (g_asyncRunning[idx])
        return;                         /* REENTRANCY GUARD: refuse to re-enter a running body */
    new B = g_asyncB[idx];              /* a live slot's block (0 IS a valid address, so no B==0 reject) */

    g_asyncFailed = failed ? 1 : 0;     /* fault channel: body checks Async_Failed() after the await */
    g_asyncErr    = err;
    g_asyncPropagate = 0;               /* bound any pending raise to a single resume cycle */

    g_asyncRunning[idx] = 1;            /* body is now on the stack */
    new done = __async_resume(B, value);
    if (g_asyncAlive[idx])              /* still allocated (a nested completion may have freed it) */
        g_asyncRunning[idx] = 0;        /* body left the stack; still parked */
    if (done)
        Async_Free(token);             /* completed -> reclaim the slot (no-op if already freed) */
}

stock Async_Resume(token, value)
{
    __async_deliver(token, value, false, 0);
}

/* Async_ResumeError(token, err): resume the coroutine reporting that the awaited
 * operation FAILED with code "err" (PawnPlus task_set_error parity). The value
 * delivered to the await is "err"; the body detects the failure with
 * Async_Failed() and reads the code with Async_Error() immediately after. */
stock Async_ResumeError(token, err)
{
    __async_deliver(token, err, true, err);
}

/* ---- MULTI-RESULT (array) delivery: PawnPlus await_arr parity -----------------
 * A single "await" yields one cell. To hand a coroutine several values at once (a
 * dialog's response+listitem+input, a query's row, ...), the resumer calls
 * Async_ResumeArr(token, values, size): the values are stashed in a per-coroutine
 * result buffer and the await returns the element COUNT. The coroutine copies the
 * values out right after the await with Async_InboxArr(dest). Pure library over the
 * existing resume seam -- no compiler change, no new opcode. */
#if !defined ASYNC_RESULT_MAX
    #define ASYNC_RESULT_MAX 16         /* max cells one Async_ResumeArr result carries */
#endif
static stock g_asyncResult[ASYNC_SLOTS][ASYNC_RESULT_MAX];  /* per-slot array inbox */
static stock g_asyncResultLen[ASYNC_SLOTS];                 /* element count last delivered */

/* Async_ResumeArr(token, values[], size): deliver an ARRAY result and resume. The
 * await the coroutine is parked on returns "size" (the element count); the values
 * are read back with Async_InboxArr(). Over-long arrays are clamped to
 * ASYNC_RESULT_MAX. A dead/unknown token is a safe no-op. */
stock Async_ResumeArr(token, const values[], size = sizeof values)
{
    new idx = token - 1;
    if (idx < 0 || idx >= ASYNC_SLOTS)
        return;
    if (size > ASYNC_RESULT_MAX)
        size = ASYNC_RESULT_MAX;
    for (new i = 0; i < size; i++)
        g_asyncResult[idx][i] = values[i];
    g_asyncResultLen[idx] = size;
    Async_Resume(token, size);          /* the await yields the element count */
}

/* __async_inbox_arr(slot, dest, size): copy the coroutine's delivered array into
 * dest, returns the element count (clamped to "size"). Internal; call the
 * Async_InboxArr() macro, which supplies the running coroutine's own slot. */
stock __async_inbox_arr(idx, dest[], size)
{
    if (idx < 0 || idx >= ASYNC_SLOTS)
        return 0;
    new n = g_asyncResultLen[idx];
    if (n > size)
        n = size;
    for (new i = 0; i < n; i++)
        dest[i] = g_asyncResult[idx][i];
    return n;
}
/* Async_InboxArr(dest): inside a coroutine body, right after an await completed by
 * Async_ResumeArr, copy the delivered array into "dest"; returns the element count.
 * (A macro so __async_self() resolves in the coroutine's own frame, like
 * Async_Self().) */
#define Async_InboxArr(%0) __async_inbox_arr(async_slot_of_addr(__async_self()), (%0), sizeof (%0))

/* Async_Failed() / Async_Error(): inspect the fault channel right after an await
 * -- Async_Failed() is nonzero iff that await was completed via
 * Async_ResumeError; Async_Error() is the code. Valid only in the run
 * immediately following the await. */
#define Async_Failed() (g_asyncFailed)
#define Async_Error()  (g_asyncErr)

/* __async_clearfault(_): the compiler emits a call to this on the return-to-
 * awaiter resume path (doreturn). It EITHER raises a pending fault into the
 * awaiter -- when the inner called Async_Fail(err) just before returning, so a
 * composed "await asyncFn()" faults automatically (AUTO-RAISE) -- OR resets the
 * channel clean, so an inner that returned NORMALLY (incl. one that observed and
 * RECOVERED from a leaf fault) hands its awaiter Async_Failed()==0 rather than a
 * stale 1. The argument is ignored; it exists only so the compiler can emit the
 * call through the same one-argument path it uses for Async_FreeByAddr. */
stock __async_clearfault(_ignored)
{
    if (g_asyncPropagate)
    {
        g_asyncFailed    = 1;           /* AUTO-RAISE: awaiter's await observes the fault */
        g_asyncErr       = g_asyncPropagateErr;
        g_asyncPropagate = 0;           /* consumed for this hop (re-raise = call Async_Fail again) */
    }
    else
    {
        g_asyncFailed = 0;              /* normal return: clean channel for the awaiter */
        g_asyncErr    = 0;
    }
    #pragma unused _ignored
    return 0;
}

/* Async_Fail(err): from inside an "async" body, fault THIS coroutine's awaiter --
 * the enclosing "await asyncFn()" will see Async_Failed()/Async_Error()==err.
 * Follow it with "return" to complete the coroutine and raise into the awaiter
 * (PawnPlus task_set_error, but propagated up the compose chain). A top-level
 * coroutine has no awaiter, so Async_Fail there has no observer. */
stock Async_Fail(err)
{
    g_asyncPropagate    = 1;
    g_asyncPropagateErr = err;
}

/* --- Async_Pending(): leaf awaitable (public) -------------------------------
 * A coroutine that "await"s a value completed by an EXTERNAL event must hand its
 * own state block to the scheduler, so the event source can resume it by token.
 * "await Async_Pending()" does that from inside an "async" body: it registers the
 * RUNNING coroutine -- named by the "__async_self()" intrinsic -- remembers the
 * token in g_asyncPending, and evaluates to that token. It MUST be a macro, not a
 * stock: "__async_self()" has to be emitted INLINE in the coroutine's own frame. */
stock g_asyncPending = 0;               /* token of the most recently self-parked coroutine */

#define Async_Pending() (g_asyncPending = Async_Register(__async_self()))

/* Async_ResumeInner(value): resume the most recently self-parked coroutine with
 * "value". A convenience for single-pending demos/tests; a real event source
 * keeps its own token and calls Async_Resume(token, value) directly. */
#define Async_ResumeInner(%1) Async_Resume(g_asyncPending, %1)

/* ======================= COMBINATORS (task_all / task_any parity) ==========
 *
 * A GATE fans several outstanding async operations into ONE awaiting coroutine,
 * matching PawnPlus's task_all / task_any without a plugin -- it is pure library
 * code over the same self-park / Async_Resume seam, no compiler change, no new
 * opcode. A coroutine creates a gate, kicks off N operations, and awaits it:
 *
 *     async FetchAll()
 *     {
 *         new g = Async_All(3);            // resume once ALL 3 complete
 *         KickOff(op1, g); KickOff(op2, g); KickOff(op3, g);
 *         new total = await Async_Wait(g); // park; total = sum of the 3 results
 *         return total;
 *     }
 *
 * Each operation reports its completion with Async_GateFeed(g, value) -- that is
 * the seam a real host adapter (timer/DB/HTTP) drives, exactly like Async_Resume.
 *   * Async_All(need)  -> gate resumes the awaiter once "need" feeds arrive;
 *                         the delivered value is the SUM of all fed values.
 *   * Async_Any(need)  -> gate resumes on the FIRST feed; value = that feed's;
 *                         later feeds are ignored.
 * CONTRACT: "await Async_Wait(g)" must run (arming the gate with the running
 * coroutine's token) BEFORE any Async_GateFeed(g, ...) -- the normal order, since
 * the coroutine kicks off its operations and only then awaits. A gate slot frees
 * itself the moment its policy is met (before it resumes the awaiter), so gate
 * storage returns to baseline just like the arena. Async_LastGate() returns the
 * id of the most recently created gate (a pump/test handle, mirroring the
 * g_asyncPending convenience). */

#if !defined ASYNC_GATES
    #define ASYNC_GATES 8               /* concurrent gates; override before include */
#endif

#define ASYNC_GATE_ALL 0
#define ASYNC_GATE_ANY 1

static stock g_gatePolicy[ASYNC_GATES];  /* ASYNC_GATE_ALL | ASYNC_GATE_ANY */
static stock g_gateNeed[ASYNC_GATES];    /* feeds required before ALL fires */
static stock g_gateDone[ASYNC_GATES];    /* feeds received so far */
static stock g_gateAcc[ASYNC_GATES];     /* aggregate: sum (ALL) / first value (ANY) */
static stock g_gateAwaiter[ASYNC_GATES]; /* token of the parked coroutine (0 = not armed) */
static stock g_gateAlive[ASYNC_GATES];   /* slot in use */
static stock g_gateFired[ASYNC_GATES];   /* policy already met+delivered (ignore late feeds) */
stock g_gateLast = 0;                     /* id of the most recently created gate */

/* Async_Gate(policy, need): claim a free gate slot and return its 1-based id, or
 * 0 if the gate table is full. Prefer the Async_All / Async_Any wrappers. */
stock Async_Gate(policy, need)
{
    for (new i = 0; i < ASYNC_GATES; i++)
    {
        if (!g_gateAlive[i])
        {
            g_gatePolicy[i]  = policy;
            g_gateNeed[i]    = need;
            g_gateDone[i]    = 0;
            g_gateAcc[i]     = 0;
            g_gateAwaiter[i] = 0;
            g_gateAlive[i]   = 1;
            g_gateFired[i]   = 0;
            g_gateLast       = i + 1;
            return i + 1;               /* 1-based gate id */
        }
    }
    return 0;                           /* gate table full */
}

#define Async_All(%1) Async_Gate(ASYNC_GATE_ALL, (%1))
#define Async_Any(%1) Async_Gate(ASYNC_GATE_ANY, (%1))
#define Async_LastGate() (g_gateLast)

/* __async_gate_arm(gate, token): record the awaiting coroutine's token on the
 * gate and return that token so "await" suspends on it. Internal -- the operand
 * of Async_Wait. */
stock __async_gate_arm(gate, token)
{
    new idx = gate - 1;
    if (idx >= 0 && idx < ASYNC_GATES && g_gateAlive[idx])
        g_gateAwaiter[idx] = token;
    return token;
}

/* Async_Wait(gate): leaf awaitable. Used as "new v = await Async_Wait(g);", it
 * registers the RUNNING coroutine (via __async_self, like Async_Pending) and arms
 * the gate to resume it when the policy is met. MUST be a macro so __async_self()
 * is emitted inline in the coroutine's own frame. */
#define Async_Wait(%1) __async_gate_arm((%1), Async_Register(__async_self()))

/* Async_GateFeed(gate, value): report one operation's completion. Accumulates per
 * policy; when the policy is met it frees the gate slot and resumes the parked
 * coroutine ONCE with the aggregate. Feeds after firing, or to an unknown/free
 * gate, are safe no-ops. */
stock Async_GateFeed(gate, value)
{
    new idx = gate - 1;
    if (idx < 0 || idx >= ASYNC_GATES)
        return;                         /* unknown gate */
    if (!g_gateAlive[idx] || g_gateFired[idx])
        return;                         /* freed, or policy already delivered */

    g_gateDone[idx]++;
    if (g_gatePolicy[idx] == ASYNC_GATE_ALL)
        g_gateAcc[idx] += value;        /* ALL: aggregate = sum of every feed */
    else if (g_gateDone[idx] == 1)
        g_gateAcc[idx] = value;         /* ANY: aggregate = the first feed */

    new met = (g_gatePolicy[idx] == ASYNC_GATE_ANY) || (g_gateDone[idx] >= g_gateNeed[idx]);
    if (met)
    {
        new tok = g_gateAwaiter[idx];
        new acc = g_gateAcc[idx];
        g_gateFired[idx] = 1;
        g_gateAlive[idx] = 0;           /* release the gate slot BEFORE resuming */
        if (tok)
            Async_Resume(tok, acc);     /* wake the awaiter with the aggregate */
    }
}

/* Async_GateActive(): number of gate slots currently in use. Baseline 0; returns
 * to 0 after every gate has fired -- the gate lifetime contract. */
stock Async_GateActive()
{
    new count = 0;
    for (new i = 0; i < ASYNC_GATES; i++)
        if (g_gateAlive[i])
            count++;
    return count;
}

/* Async_GateFail(gate, err): one operation feeding the gate FAILED. Fail-fast for
 * both policies -- the gate fires immediately as a failure, freeing itself and
 * resuming the awaiter via the fault channel (Async_Failed()/Async_Error() are
 * set on the resumed body). A failure to a fired/unknown/free gate is a no-op. */
stock Async_GateFail(gate, err)
{
    new idx = gate - 1;
    if (idx < 0 || idx >= ASYNC_GATES)
        return;                         /* unknown gate */
    if (!g_gateAlive[idx] || g_gateFired[idx])
        return;                         /* freed, or already delivered */

    new tok = g_gateAwaiter[idx];
    g_gateFired[idx] = 1;
    g_gateAlive[idx] = 0;               /* release the gate slot before resuming */
    if (tok)
        Async_ResumeError(tok, err);    /* wake the awaiter as a failure */
}
