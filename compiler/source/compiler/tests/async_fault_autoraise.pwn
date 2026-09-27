#include <console>
#include <async>

// IMPLICIT fault auto-raise across a composed await. Before this, a leaf fault that
// an inner "async" fn IGNORED (never read Async_Failed()) was dropped when the inner
// returned -- its awaiter saw a clean channel. Now an unobserved leaf fault is
// AUTO-RAISED into the awaiter, while an inner that OBSERVES and recovers still hands
// a clean channel. Explicit Async_Fail (async_fault_raise) is unchanged. The
// distinction is tracked by whether the body read the fault (Async_Failed/Error mark
// it observed), consumed on the return-to-awaiter path.

async InnerIgnore()
{
    new x = await Async_Pending();       // leaf: completed as an ERROR by main
    return x + 1;                        // never checks the fault -> it AUTO-RAISES
}

async InnerRecover()
{
    new x = await Async_Pending();
    if (Async_Failed())                  // OBSERVES the fault...
        return 777;                      // ...and recovers -> awaiter sees NO fault
    return x;
}

async Outer(mode)
{
    new r;
    if (mode == 1) r = await InnerIgnore();
    else           r = await InnerRecover();
    if (Async_Failed())
        printf("mode=%d caught err=%d\n", mode, Async_Error());
    else
        printf("mode=%d ok r=%d\n", mode, r);
}

main()
{
    Async_Start(Outer, 1);
    Async_ResumeError(g_asyncPending, 55);   // fail inner's leaf -> ignored -> auto-raise 55
    Async_Start(Outer, 2);
    Async_ResumeError(g_asyncPending, 66);   // fail inner's leaf -> recovered -> no raise
    printf("active=%d\n", Async_ActiveCount());
}
