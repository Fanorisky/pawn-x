#include <console>
#include <async>
#include <foreach>

// An "await" may appear inside a "foreach" over a PLAIN SET in an "async"
// coroutine: the loop variable and the walk state are lifted into the state
// block (gen_reserved==4 for async), so they survive the suspend. The set walk
// resumes correctly across each await, exactly like a plain while-loop await.

new g_set[8];

async Consume()
{
    new total = 0;
    foreach (new x : g_set)
    {
        new got = await Async_Pending();   // suspend inside the set walk
        total += x + got;                  // x (loop var) and total survive
    }
    printf("total=%d\n", total);
    return total;
}

main()
{
    setinit(g_set);
    setadd(g_set, 1);
    setadd(g_set, 2);
    setadd(g_set, 4);                       // 1, 2, 4
    Async_Start(Consume);
    Async_ResumeInner(100);                 // await #1 -> got=100
    Async_ResumeInner(200);                 // await #2 -> got=200
    Async_ResumeInner(300);                 // await #3 -> got=300
    // total = (1+2+4) + (100+200+300) = 607
}
