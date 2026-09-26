#include <console>
#include <async>

// Promoted from experiment 012's async_basic spike, now on the finalized <async>
// PUBLIC API (Async_Start / Async_Resume) instead of the raw __async_* intrinsics.
// A single await suspends the coroutine back to its caller; a synthetic pump --
// standing in for a real timer/dialog/DB completion (out of MVP scope) -- later
// calls Async_Resume with the awaited result. The "(started; suspended...)" line
// prints BEFORE the resume, proving control genuinely returned to the caller.
//   base = 100 (set before the await, survives it); s = 7 + 100 = 107;
//   score = s + base = 207.
new g_task;

// A stand-in async "operation": in a real host this schedules work and arranges
// Async_Resume(token, value) on completion; here it just computes the value.
AddScore(a, b) { return a + b; }

async GetScore(playerid)
{
    new base = 100;
    new s = await AddScore(playerid, base);   // suspend; pump resumes with 107
    printf("score=%d\n", s + base);           // 'base' survived the await
}

main()
{
    g_task = Async_Start(GetScore, 7);        // runs to the await, returns here
    printf("%s\n", "(started; suspended at await, control returned to caller)");
    Async_Resume(g_task, AddScore(7, 100));   // synthetic pump delivers 107 -> 207
}
