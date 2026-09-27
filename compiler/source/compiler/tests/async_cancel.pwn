#include <console>
#include <async>

// Cancellation parity (task_delete): two shapes.
//  * Async_Cancel (SOFT) resumes a parked coroutine through the fault channel with
//    the reserved ASYNC_ERR_CANCELLED code, so the body observes Async_Cancelled()
//    right after its await, runs cleanup, and returns -- completing and freeing it.
//  * Async_Kill (HARD) frees the slot immediately; the body's post-await code never
//    runs. It refuses (returns 0) a running body or a dead/unknown token.

new g_cleaned = 0;

async Worker()
{
    new got = await 0;                   // park until cancelled/resumed
    if (Async_Cancelled())
    {
        g_cleaned = 1;                   // cooperative cleanup on cancel
        printf("worker cancelled=%d\n", Async_Cancelled());
        return;
    }
    printf("worker got=%d\n", got);
}

async Worker2()
{
    new got = await 0;
    printf("worker2 got=%d (should NOT print if killed)\n", got);
}

main()
{
    new t1 = Async_Start(Worker);
    printf("start active=%d\n", Async_ActiveCount());
    Async_Cancel(t1);                    // soft cancel -> body cleans up + returns
    printf("after cancel cleaned=%d active=%d\n", g_cleaned, Async_ActiveCount());

    new t2 = Async_Start(Worker2);
    new killed = Async_Kill(t2);         // hard kill -> body never resumes
    printf("after kill killed=%d active=%d\n", killed, Async_ActiveCount());
    printf("kill dead token=%d\n", Async_Kill(t2));   // already freed -> 0
}
