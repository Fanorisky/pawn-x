#include <console>
#include <async>

// Two DIFFERENT async functions parked at the same time and resumed by the
// GENERIC (token-dispatched) path in REVERSE order. This exercises the
// call.pri indirect resume: the scheduler holds each coroutine's entry address
// in its state block B[ASYNC_ENTRY_SLOT] and dispatches to it by token, so two
// distinct coroutine bodies resume correctly from one generic Async_Resume.
async WorkerA(id)
{
    new base = 7000;                 // distinct per-body local, survives the await
    new got = await 0;               // suspend; scheduler delivers the value on resume
    printf("A id=%d got=%d sum=%d\n", id, got, base + got);
}

async WorkerB(id)
{
    new base = 9000;                 // different constant proves state isolation
    new got = await 0;
    printf("B id=%d got=%d sum=%d\n", id, got, base + got);
}

main()
{
    new t1 = Async_Start(WorkerA, 1);
    new t2 = Async_Start(WorkerB, 2);
    printf("(parked)\n");
    Async_Resume(t2, 222);           // resume the SECOND one first
    Async_Resume(t1, 111);
}
