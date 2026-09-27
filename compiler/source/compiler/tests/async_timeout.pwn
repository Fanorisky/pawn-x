#include <console>
#include <async>

// Race-free "await with timeout" (task_set_error_ms parity). A coroutine awaits an
// Async_Any gate; whichever reaches it first -- the real operation (Async_GateFeed)
// or the timeout (Async_GateFail) -- wins, and the gate ignores every later arrival.
// The open.mp adapter's Async_TimeoutGate(gate, ms) is just a SetTimerEx that calls
// Async_GateFail; here the feed/fail are driven synthetically so the LOGIC is
// verifiable off-host (the timer itself needs the live server).
async OpWithTimeout(mode)
{
    new g = Async_Any(2);                // resume on the FIRST arrival at the gate
    new r = await Async_Wait(g);
    if (Async_TimedOut())
        printf("mode=%d timeout=1\n", mode);
    else
        printf("mode=%d ok r=%d\n", mode, r);
}

main()
{
    Async_Start(OpWithTimeout, 1);
    Async_GateFeed(Async_LastGate(), 77);            // real op wins

    Async_Start(OpWithTimeout, 2);
    Async_GateFail(Async_LastGate(), ASYNC_ERR_TIMEOUT);  // timeout wins

    Async_Start(OpWithTimeout, 3);
    new g3 = Async_LastGate();
    Async_GateFail(g3, ASYNC_ERR_TIMEOUT);           // timeout fires first
    Async_GateFeed(g3, 999);                         // late success -> ignored

    printf("done active=%d gates=%d\n", Async_ActiveCount(), Async_GateActive());
}
