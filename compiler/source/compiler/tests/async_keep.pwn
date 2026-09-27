#include <console>
#include <async>

// task_keep parity: a task marked Async_Keep is NOT freed when it completes --
// its slot stays alive and its final RETURN VALUE stays readable with
// Async_Result until Async_Release. This exercises the compiler's completion-
// value capture (__async_complete records the top-level coroutine's return
// value, which the free-on-complete path used to discard) and the re-resume
// guard (a completed+kept task refuses a second resume instead of re-running).
async Compute(x)
{
    new base = 100;              // lifted local, survives the await
    new got = await 0;           // suspend; resumed with a value
    return base + got + x;       // top-level return value -> captured on completion
}

main()
{
    new t = Async_Start(Compute, 5);
    Async_Keep(t);                       // keep the result after completion
    printf("start active=%d done=%d\n", Async_ActiveCount(), Async_Done(t));
    Async_Resume(t, 20);                 // completes: 100 + 20 + 5 = 125
    printf("done=%d has=%d result=%d active=%d\n",
           Async_Done(t), Async_HasResult(t), Async_Result(t), Async_ActiveCount());
    Async_Resume(t, 999);                // completed+kept -> a re-resume is a no-op
    printf("reresume result=%d active=%d\n", Async_Result(t), Async_ActiveCount());
    Async_Release(t);                    // release the kept task -> slot returns to the pool
    printf("release active=%d has=%d\n", Async_ActiveCount(), Async_HasResult(t));
}
