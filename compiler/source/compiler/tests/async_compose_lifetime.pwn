#include <console>
#include <async>

// Composed-await lifetime (experiment 012, task 3 fix round 1). The headline
// "await asyncFn(args)" path used to allocate the inner coroutine's B with a raw
// modheap (an unbounded HEA leak per composed await), and a top-level coroutine
// that completes through the return-to-awaiter call.pri chain was never freed --
// so after ASYNC_MAX (16) composed chains the arena would exhaust and Async_Start
// would silently start nothing. This runs a composed chain 20 > ASYNC_MAX times:
// if any inner B leaked or any completed coroutine was not freed, the arena would
// run out and later iterations would not complete -> completed < 20.

new g_completed = 0;

async Inner(x)
{
    new bump = await __pending();     // external suspension: main resumes it
    return x + bump;                  // return-to-awaiter: resumes Outer with this
}

async Outer(base)
{
    new r = await Inner(base);        // ergonomic compose: inner B now from the arena
    if (r == base + 100)
        g_completed++;
}

main()
{
    for (new i = 1; i <= 20; i++)     // 20 > ASYNC_MAX (16): proves free + reuse
    {
        Async_Start(Outer, i * 10);   // Outer starts, awaits Inner, which self-parks
        Async_ResumeInner(100);       // complete Inner -> unwinds up to Outer
    }
    printf("compose completed=%d active=%d\n", g_completed, Async_ActiveCount());
}
