#include <console>
#include <async>

// Fault propagation (experiment 012 phase 3): task_set_error parity, native and
// plugin-free. A completing operation can report FAILURE instead of a value:
//   Async_ResumeError(token, err) -> the coroutine's await observes it via
//   Async_Failed()/Async_Error() right after resuming (the leaf-await error model).
//   Async_GateFail(gate, err) -> fail-fast a combinator gate, resuming its awaiter
//   as a failure through the same channel.

async DoOp()
{
    new v = await 0;                      // suspend; host resumes us by our start token
    if (Async_Failed())                   // check the fault channel right after the await
    {
        printf("op FAILED err=%d\n", Async_Error());
        return -1;
    }
    printf("op ok v=%d\n", v);
    return v;
}

async DoAll()
{
    new g = Async_All(2);                 // wait for 2 operations
    new total = await Async_Wait(g);
    if (Async_Failed())
    {
        printf("all FAILED err=%d\n", Async_Error());
        return -1;
    }
    printf("all ok total=%d\n", total);
    return total;
}

main()
{
    // --- leaf failure: the awaited op is completed as an error.
    new t1 = Async_Start(DoOp);
    printf("(op1 parked)\n");
    Async_ResumeError(t1, 42);            // fail it with code 42

    // --- leaf success: same coroutine shape, completed normally -> no stale error.
    new t2 = Async_Start(DoOp);
    printf("(op2 parked)\n");
    Async_Resume(t2, 99);                 // Async_Failed() must be 0 here

    // --- gate failure: one feeder fails -> the whole ALL gate fails fast.
    Async_Start(DoAll);
    new g = Async_LastGate();
    printf("(all parked)\n");
    Async_GateFeed(g, 10);                // one success...
    Async_GateFail(g, 7);                 // ...then a failure fails the gate

    printf("active=%d gates=%d\n", Async_ActiveCount(), Async_GateActive());
}
