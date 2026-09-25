#include <console>
#include <async>

// Resuming a token that was never parked (or already completed) must be a SAFE
// NO-OP: no crash, no jump to garbage, control simply continues. The sentinel
// printed after the bogus resumes proves execution flowed straight through.
async Worker(id)
{
    new got = await 0;
    printf("worker id=%d got=%d\n", id, got);
}

main()
{
    new t = Async_Start(Worker, 5);

    // unknown tokens: 0 (never a valid token), a huge out-of-range value, and a
    // negative value -> each is ignored.
    Async_Resume(0, 1);
    Async_Resume(9999, 2);
    Async_Resume(-3, 3);

    // real resume completes the coroutine
    Async_Resume(t, 42);

    // resuming the SAME token again after completion is also a no-op
    Async_Resume(t, 99);

    printf("done\n");
}
