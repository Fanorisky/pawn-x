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
