#if defined _inc_async
    #endinput
#endif
#define _inc_async

/* async.inc -- in-script scheduler registry + fixed arena for native
 * async/await (experiment 012, tasks 1-3).
 *
 * The compiler emits an "async" function as a coroutine whose whole state lives
 * in a heap-like "state block" B. Three intrinsics drive it:
 *   __async_start(Fn, args...) -> B      obtain a B block from the arena
 *                                        (Async_Alloc), run Fn to its first
 *                                        "await", record Fn's code entry in B,
 *                                        return B (0 if the arena is full).
 *   __async_resume(B, value)   -> done?  deliver "value" to the parked coroutine
 *                                        and resume it by its stored code entry
 *                                        via the "call.pri" indirect call; yields
 *                                        1 if the coroutine completed, else 0.
 *   __async_self()             -> B      the running coroutine's own B block.
 *
 * TASK 3 -- lifetime & safety.  A coroutine's B outlives the frame that started
 * it, and coroutines complete in ARBITRARY order.  The AMX heap is LIFO (a single
 * HEA cursor moved by "modheap"/"heap"), so an arbitrary non-top block CANNOT be
 * released by lowering HEA without corrupting the heap.  We therefore stop using
 * the heap for B entirely: B blocks come from a FIXED ARENA (g_arena) that lives
 * in the data segment.  A block is just a slot in that array; the TOKEN is the
 * slot index (1-based).  Async_Alloc() hands out a free slot's stable address,
 * Async_Free() returns a slot to the free pool (double-free is a no-op), so
 * starting+completing N coroutines in sequence reuses slots and never grows
 * storage.  This sidesteps the LIFO-heap problem completely.
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

/* --- self-parking helper (task 2: return-to-awaiter composition) ------------
 * A coroutine that "await"s a value completed by an EXTERNAL event must hand its
 * own state block to the scheduler, so the event source can resume it by token.
 * __pending() does that from inside an "async" body: it registers the RUNNING
 * coroutine -- named by the "__async_self()" intrinsic -- remembers the token in
 * g_asyncPending, and evaluates to that token. It MUST be a macro, not a stock:
 * "__async_self()" has to be emitted INLINE in the coroutine's own frame. */
stock g_asyncPending = 0;               /* token of the most recently self-parked coroutine */

#define __pending() (g_asyncPending = Async_Register(__async_self()))

/* Async_ResumeInner(value): resume the most recently self-parked coroutine with
 * "value". A convenience for single-pending demos/tests; a real event source
 * keeps its own token and calls Async_Resume(token, value) directly. */
#define Async_ResumeInner(%1) Async_Resume(g_asyncPending, %1)
