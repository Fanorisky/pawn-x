/* async_omp.inc -- open.mp HOST ADAPTER for the native async runtime.
 *
 * async.inc ships the coroutine runtime and the Async_Resume(token, value) seam
 * but NO real event source (a "synthetic pump" drives it in the compiler tests).
 * This companion bridges that seam to REAL open.mp events so an "async" coroutine
 * can await actual server time and callbacks -- closing the "no host adapter"
 * MVP non-goal on a live host:
 *
 *   async Countdown()
 *   {
 *       await Async_Ms(1000);        // suspend on a REAL open.mp timer
 *       print("one second later");
 *   }
 *   Async_Start(Countdown);          // returns immediately; the timer resumes it
 *
 *   * Async_Ms(ms)   -> await a one-shot open.mp timer (SetTimerEx). Resumes with 0.
 *   * Async_Ticks(n) -> await n server ticks (a timer of n*~... ; here n ms as the
 *                       coarse tick proxy -- open.mp drives timers off the tick loop).
 *   * Async_Self()   -> the running coroutine's own STABLE token (its physical arena
 *                       slot), so re-arming across several awaits never leaks an alias
 *                       slot the way Async_Register would.
 *
 * A callback awaitable (resume on a named public firing) reuses the same seam:
 * store Async_Self() somewhere your callback can reach and call Async_Resume there.
 *
 *   * Async_Timeout(token, ms[, err])       -> fault a task after ms (task_set_error_ms).
 *   * Async_TimeoutGate(gate, ms[, err])    -> fault a GATE after ms (race-free timeout).
 *
 * This adapter also enables real completion-callback dispatch: it defines
 * ASYNC_HOST_CALLBACKS before including async.inc so Async_Bind fires its public
 * through CallLocalFunction (a host native). Include your open.mp/SA-MP header
 * (which declares SetTimerEx and CallLocalFunction) before this file.
 */

#if !defined ASYNC_HOST_CALLBACKS
    #define ASYNC_HOST_CALLBACKS        /* Async_Bind dispatches via CallLocalFunction on the host */
#endif
#include "async.inc"

/* Async_Self(): the running coroutine's stable token = its physical arena slot
 * (async_slot_of_addr maps its block B back to the slot). Unlike Async_Register
 * (which mints a fresh alias slot every call, leaking one per await when a
 * coroutine re-arms), this reuses the one slot the coroutine already owns, so a
 * coroutine can await any number of times and still return to baseline. */
#define Async_Self() (async_slot_of_addr(__async_self()) + 1)

/* The public every host awaitable resumes through: open.mp calls it with the
 * parked coroutine's token, and it drives one resume. */
forward __async_on_timer(token);
public __async_on_timer(token)
{
    Async_Resume(token, 0);
}

/* __async_ms(ms, token): arm a one-shot open.mp timer to resume 'token' after
 * 'ms' milliseconds, and return the token so "await" suspends on it. */
stock __async_ms(ms, token)
{
    SetTimerEx("__async_on_timer", ms, false, "i", token);
    return token;
}

/* await Async_Ms(ms): suspend the running coroutine until a real timer fires. */
#define Async_Ms(%1) __async_ms((%1), Async_Self())

/* await Async_Ticks(n): coarse tick wait (open.mp drives timers off its tick
 * loop; one "tick" here is one millisecond of timer resolution). */
#define Async_Ticks(%1) __async_ms((%1), Async_Self())

/* ===================== TIMEOUT (task_set_error_ms parity) ===================
 * A timer that reports a FAILURE after a delay. Two forms:
 *
 *   Async_Timeout(token, ms) -- fault the task directly after ms. Simple, but note
 *     the classic timer race: if the task completes first and its slot is reused,
 *     the late timer would fault the REUSED coroutine. Use it only when the token
 *     cannot be reused before the timer is moot, or prefer the gate form below.
 *
 *   Async_TimeoutGate(gate, ms) -- fault a GATE after ms. RACE-FREE: whichever of
 *     the real op (Async_GateFeed) or the timeout (Async_GateFail) reaches the gate
 *     first wins, and the gate ignores every later arrival. This is the recommended
 *     "await with timeout":
 *
 *         new g = Async_Any(2);
 *         StartRealOp(g);                       // -> Async_GateFeed(g, value) on success
 *         Async_TimeoutGate(g, 1000);           // -> Async_GateFail(g, ...) after 1s
 *         new r = await Async_Wait(g);
 *         if (Async_TimedOut()) { ...handle the timeout... }
 */
forward __async_on_timeout(token, err);
public __async_on_timeout(token, err)
{
    Async_ResumeError(token, err);
}

stock Async_Timeout(token, ms, err = ASYNC_ERR_TIMEOUT)
{
    SetTimerEx("__async_on_timeout", ms, false, "ii", token, err);
    return token;
}

forward __async_on_timeout_gate(gate, err);
public __async_on_timeout_gate(gate, err)
{
    Async_GateFail(gate, err);
}

stock Async_TimeoutGate(gate, ms, err = ASYNC_ERR_TIMEOUT)
{
    SetTimerEx("__async_on_timeout_gate", ms, false, "ii", gate, err);
    return gate;
}

