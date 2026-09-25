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
 *       new s = await AddScore(playerid, base);   // suspend; resume w/ result
 *       printf("score=%d\n", s + base);   // 'base' still holds 100 here
 *       return s + base;                  // delivers to this fn's awaiter
 *   }
 *
 *   new t = Async_Start(GetScore, 7);     // runs to the await, returns here
 *   Async_Resume(t, 107);                 // a pump/host completion resumes it
 *
 *   "await asyncFn(args)" composes: it starts asyncFn and parks the caller until
 *   asyncFn's "return" delivers its value straight back (return-to-awaiter).
 *
 * ============================ v1 LIMITS ==================================
 *   * Scalar locals only across an await (array/string live across a suspend is
 *     rejected -- error 096, same envelope as "yield" v1).
 *   * Linear bodies (sequential awaits, if, simple loops). "await" inside a
 *     "foreach" is rejected (error 099); "foreach" over an async fn is rejected
 *     (error 267).
 *   * Single compilation unit (one .amx).
 *
 * ============================ NON-GOALS (MVP) ============================
 *   * A real host adapter (timer/dialog/DB/HTTP) -- Async_Resume is the seam; no
 *     adapter ships in the MVP.
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
 * from a FIXED ARENA (g_arena) in the data segment: a block is a slot, the TOKEN
 * is its 1-based index, Async_Alloc() hands out a free slot's stable address and
 * Async_Free() returns it to the pool (double-free is a no-op), so starting+
 * completing N coroutines reuses slots and never grows storage.
 */

#include <core>                 /* heapspace() -- used by the lifetime test */

#if !defined ASYNC_MAX
    #define ASYNC_MAX 16            /* max simultaneously-parked coroutines */
#endif

/* Cells reserved per arena block. Must be >= gen_reserved(async)=4 plus the
 * largest number of lifted params+locals of any "async" function in the program.
 * DOCUMENTED CAP: an "async" function with more than (ASYNC_BLOCK_CELLS - 4)
 * lifted params+locals overflows its block; raise ASYNC_BLOCK_CELLS (before
 * including <async>) for such programs. 64 comfortably covers the MVP bodies. */
#if !defined ASYNC_BLOCK_CELLS
    #define ASYNC_BLOCK_CELLS 64
#endif

/* The fixed arena: ASYNC_MAX blocks laid out contiguously (a flat 1D array so
 * its address arithmetic has no indirection vector). Slot i's block base is
 * &g_arena[i*ASYNC_BLOCK_CELLS]. */
static stock g_arena[ASYNC_MAX * ASYNC_BLOCK_CELLS];

static stock g_asyncB[ASYNC_MAX];       /* slot -> state block B (0 = empty) */
static stock g_asyncAlive[ASYNC_MAX];   /* slot -> 1 while the coroutine is allocated/parked */
static stock g_asyncRunning[ASYNC_MAX]; /* slot -> 1 while its body is ON THE STACK (reentrancy guard) */

/* Slot chosen by the most recent Async_Alloc, so Async_Start can turn the
 * returned block address back into its token. -1 means the arena was full. */
stock g_asyncAllocSlot = -1;

/* Async_Alloc(): claim a free arena slot and return the STABLE data address of
 * its block (which the compiler uses as B). Records the chosen slot in
 * g_asyncAllocSlot so Async_Start can recover the token. Returns -1 (and sets
 * g_asyncAllocSlot = -1) when the arena is exhausted OR when the requested block
 * does not fit a slot -- the compiler guards on this and starts no coroutine in
 * that case. NOTE the sentinel is -1, not 0: a block can legitimately sit at data
 * address 0 (g_arena may be the first global), so 0 is a VALID block address and
 * must never mean "invalid". "blkcells" is the compiler-known size of this
 * coroutine's block (gen_reserved + lifted params/locals); it is bounds-checked
 * against ASYNC_BLOCK_CELLS so an over-large coroutine yields the clean -1
 * sentinel instead of corrupting the next slot. The block stride is computed from
 * the arena itself (__addressof differences), so it is cell-size portable. */
stock Async_Alloc(blkcells)
{
    if (blkcells > ASYNC_BLOCK_CELLS)
    {
        g_asyncAllocSlot = -1;
        return -1;                      /* block too large for a slot: clean sentinel */
    }
    new base   = __addressof(g_arena[0]);
    new stride = __addressof(g_arena[ASYNC_BLOCK_CELLS]) - base;  /* one block, in bytes */
    for (new i = 0; i < ASYNC_MAX; i++)
    {
        if (!g_asyncAlive[i])
        {
            new B = base + i * stride;
            g_asyncAlive[i]   = 1;
            g_asyncRunning[i] = 0;
            g_asyncB[i]       = B;
            g_asyncAllocSlot  = i;
            return B;
        }
    }
    g_asyncAllocSlot = -1;
    return -1;                          /* arena full (sentinel: never a valid address) */
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
    for (new i = 0; i < ASYNC_MAX; i++)
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
    if (idx < 0 || idx >= ASYNC_MAX)
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
 * Async_Resume -- still reclaims its slot. An address that is not an arena block
 * base (out of range, or not slot-aligned) is a safe no-op, as is a double-free. */
stock Async_FreeByAddr(B)
{
    new base   = __addressof(g_arena[0]);
    new stride = __addressof(g_arena[ASYNC_BLOCK_CELLS]) - base;
    new off    = B - base;
    if (off < 0)
        return;                         /* below the arena: not ours */
    new idx = off / stride;
    if (idx >= ASYNC_MAX)
        return;                         /* above the arena: not ours */
    if (off - idx * stride != 0)
        return;                         /* not a block base: defensive no-op */
    Async_Free(idx + 1);                /* double-free safe */
}

/* Async_ActiveCount(): number of arena slots currently allocated (parked or
 * running). Baseline is 0; after every started coroutine has completed it must
 * return to 0 again -- the binding lifetime contract. */
stock Async_ActiveCount()
{
    new count = 0;
    for (new i = 0; i < ASYNC_MAX; i++)
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
 * coroutine completes on this resume its token is freed automatically. */
stock Async_Resume(token, value)
{
    new idx = token - 1;
    if (idx < 0 || idx >= ASYNC_MAX)
        return;                         /* unknown token */
    if (!g_asyncAlive[idx])
        return;                         /* never parked / already completed */
    if (g_asyncRunning[idx])
        return;                         /* REENTRANCY GUARD: refuse to re-enter a running body */
    new B = g_asyncB[idx];              /* a live slot's block (0 IS a valid address, so no B==0 reject) */

    g_asyncRunning[idx] = 1;            /* body is now on the stack */
    new done = __async_resume(B, value);
    if (g_asyncAlive[idx])              /* still allocated (a nested completion may have freed it) */
        g_asyncRunning[idx] = 0;        /* body left the stack; still parked */
    if (done)
        Async_Free(token);             /* completed -> reclaim the slot (no-op if already freed) */
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
