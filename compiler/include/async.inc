#if defined _inc_async
    #endinput
#endif
#define _inc_async

/* async.inc -- minimal in-script scheduler registry for native async/await
 * (experiment 012, task 1).
 *
 * The compiler emits an "async" function as a coroutine whose whole state lives
 * in a heap "state block" B. Two intrinsics drive it:
 *   __async_start(Fn, args...) -> B      allocate B, run Fn to its first "await",
 *                                        record Fn's code entry in B, return B.
 *   __async_resume(B, value)   -> done?  deliver "value" to the parked coroutine
 *                                        and resume it by its stored code entry
 *                                        via the "call.pri" indirect call; yields
 *                                        1 if the coroutine completed, else 0.
 *
 * This file turns the raw B pointer into an opaque integer TOKEN and keeps a
 * fixed-size token->B table, so a scheduler can park many coroutines of
 * different "async" functions at once and resume any of them by token. Resuming
 * an unknown or already-completed token is a safe no-op.
 */

#if !defined ASYNC_MAX
    #define ASYNC_MAX 16            /* max simultaneously-parked coroutines */
#endif

static stock g_asyncB[ASYNC_MAX];       /* token-1 -> state block B (0 = empty) */
static stock g_asyncAlive[ASYNC_MAX];   /* token-1 -> 1 while the coroutine is parked */

/* Async_Register(B): record an already-started coroutine block B in a free slot
 * and return its token (>= 1), or 0 if the table is full. */
stock Async_Register(B)
{
    for (new i = 0; i < ASYNC_MAX; i++)
    {
        if (!g_asyncAlive[i])
        {
            g_asyncB[i] = B;
            g_asyncAlive[i] = 1;
            return i + 1;               /* token: 1-based so 0 is always invalid */
        }
    }
    return 0;                           /* table full */
}

/* Async_Start(Fn, args...): start coroutine Fn (runs to its first "await") and
 * register it; returns a token. Fn must name an "async" function. Implemented as
 * a macro because the start intrinsic needs the callee named at the call site. */
#define Async_Start(%1) Async_Register(__async_start(%1))

/* Async_Complete(token): deactivate a token so future resumes are no-ops. */
stock Async_Complete(token)
{
    new idx = token - 1;
    if (idx < 0 || idx >= ASYNC_MAX)
        return;
    g_asyncAlive[idx] = 0;
    g_asyncB[idx] = 0;
}

/* Async_Resume(token, value): deliver "value" to the coroutine parked under
 * "token" and resume it. Resuming an out-of-range, never-parked or already
 * completed token is a safe no-op (no crash, no jump to garbage). When the
 * coroutine completes on this resume, its token is deactivated automatically. */
stock Async_Resume(token, value)
{
    new idx = token - 1;
    if (idx < 0 || idx >= ASYNC_MAX)
        return;                         /* unknown token */
    if (!g_asyncAlive[idx])
        return;                         /* never parked / already completed */
    new B = g_asyncB[idx];
    if (B == 0)
        return;                         /* defensive: no block */
    if (__async_resume(B, value))       /* nonzero => coroutine ran to completion */
        Async_Complete(token);
}

/* --- self-parking helper (task 2: return-to-awaiter composition) ------------
 * A coroutine that "await"s a value completed by an EXTERNAL event must hand its
 * own state block to the scheduler, so the event source can resume it by token.
 * __pending() does that from inside an "async" body: it registers the RUNNING
 * coroutine -- named by the "__async_self()" intrinsic -- remembers the token in
 * g_asyncPending, and evaluates to that token. It MUST be a macro, not a stock:
 * "__async_self()" has to be emitted INLINE in the coroutine's own frame (a
 * called stock would report its own frame, not the coroutine's state block).
 * NOTE: not "static" -- it is referenced from the including file via __pending(),
 * and a "static" global is file-local; "stock" alone stays globally visible yet
 * is discarded (no unused warning) in scripts that never self-park. */
stock g_asyncPending = 0;               /* token of the most recently self-parked coroutine */

#define __pending() (g_asyncPending = Async_Register(__async_self()))

/* Async_ResumeInner(value): resume the most recently self-parked coroutine with
 * "value". A convenience for single-pending demos/tests; a real event source
 * keeps its own token and calls Async_Resume(token, value) directly. */
#define Async_ResumeInner(%1) Async_Resume(g_asyncPending, %1)
