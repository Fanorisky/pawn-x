#include <console>
#include <async>

// task_bind / task_detach parity: a completion callback bound to a task fires with
// (token, result) when the task completes. The host dispatch is CallLocalFunction
// (compiled only under ASYNC_HOST_CALLBACKS, since that native is absent from
// pawnruns and the bundled includes); the binding LOGIC is verifiable off-host via
// g_asyncFiredTok / g_asyncFiredVal, which record the most recent fire regardless.

async Compute(x)
{
    new got = await 0;
    return got + x;                      // completion value -> passed to the binding
}

main()
{
    new t = Async_Start(Compute, 3);
    Async_Bind(t, "OnDone");
    Async_Resume(t, 40);                 // completes 43 -> fire OnDone(token, 43)
    printf("fired tok=%d val=%d\n", g_asyncFiredTok, g_asyncFiredVal);

    g_asyncFiredTok = 0; g_asyncFiredVal = 0;
    new t2 = Async_Start(Compute, 1);
    Async_Bind(t2, "OnDone");
    Async_Detach(t2);                    // unbind -> no fire on completion
    Async_Resume(t2, 10);
    printf("detach tok=%d val=%d\n", g_asyncFiredTok, g_asyncFiredVal);

    g_asyncFiredTok = 0; g_asyncFiredVal = 0;
    new t3 = Async_Start(Compute, 100);
    Async_Bind(t3, "OnDone");
    Async_Keep(t3);                      // bind + keep: fire recorded AND result kept
    Async_Resume(t3, 5);                 // completes 105
    printf("keepbind tok=%d val=%d result=%d\n", g_asyncFiredTok, g_asyncFiredVal, Async_Result(t3));
    Async_Release(t3);
}
