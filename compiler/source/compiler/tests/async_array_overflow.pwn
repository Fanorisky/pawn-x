#include <console>
#include <async>

// Task 1 review focus: a coroutine whose lifted block EXCEEDS the largest size
// class (ASYNC_LARGEST_CELLS = 512 cells). "new buf[600]" makes blkcells > 512,
// so Async_Alloc returns the -1 no-start sentinel, Async_Start yields token 0,
// and the coroutine NEVER runs. main must continue cleanly and uncorrupted:
// the "ran" flag stays 0, the arena stays empty (active 0 -> 0, nothing leaked),
// and resuming the invalid token 0 is a safe no-op.

new g_ran = 0;

async TooBig()
{
    new buf[600];                 // exceeds the largest class -> block won't fit
    buf[0] = 1;
    new got = await 0;
    g_ran = 1;                     // must NEVER execute (coroutine never started)
    printf("SHOULD-NOT-RUN got=%d buf0=%d\n", got, buf[0]);
}

main()
{
    new before = Async_ActiveCount();
    new t = Async_Start(TooBig);
    Async_Resume(t, 42);           // resuming token 0 is a safe no-op
    printf("no-start token=%d ran=%d active=%d->%d ok=%d\n",
           t, g_ran, before, Async_ActiveCount(), (t == 0 && g_ran == 0));
}
