#include <console>
#include <async>

// Reentrancy guard (experiment 012, task 3). An awaited operation may complete
// SYNCHRONOUSLY: the resume path ends up calling Async_Resume for the SAME
// coroutine from within its own still-running body. Re-entering a live coroutine
// frame would corrupt its state (and here would recurse without bound, since the
// resume CIP after the await is still armed). The per-slot "running" flag makes
// such a re-entrant resume a safe NO-OP, so the body runs exactly once and the
// accounting stays coherent.

new g_selfToken;

async Reenter()
{
    new got = await 0;                      // suspend; main resumes this by token
    // We are now RUNNING. Try to resume ourselves synchronously -- the guard must
    // reject it (no re-entry, no second run, no runaway recursion).
    new activeBefore = Async_ActiveCount();
    Async_Resume(g_selfToken, 777);         // re-entrant: MUST be a no-op
    new guard_held = (activeBefore == Async_ActiveCount());
    printf("reenter got=%d guard_held=%d\n", got, guard_held);
}

main()
{
    g_selfToken = Async_Start(Reenter);
    printf("(reentrant parked) token=%d\n", g_selfToken);
    Async_Resume(g_selfToken, 42);          // resume; body attempts a synchronous self-resume
    printf("final active=%d\n", Async_ActiveCount());
}
