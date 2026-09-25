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
 */

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
