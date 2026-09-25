#include <console>
#include <core>
#include <async>

// Lifetime accounting (experiment 012, task 3). Starting and fully completing a
// coroutine must RECLAIM its state block: the arena slot returns to the free
// pool, so N coroutines started+completed in sequence reuse slots and never grow
// storage. Two things are asserted:
//   * Async_ActiveCount() returns to its baseline (0) after each completion, and
//   * heapspace() is unchanged across the whole run -- proof that B no longer
//     leaks on the heap (the spike's modheap grew HEA on every start and never
//     freed it; the arena lives in the data segment and moves HEA not at all).
// Slot reuse is shown by the token: a freed slot is handed back out, so every
// start reports the same token (1) instead of climbing 1,2,3,4.

new g_h1, g_h2;                       // heapspace snapshots (globals: no stack impact)

async Life(id)
{
    new got = await 0;                // suspend; resumed directly by its own token
    printf("life id=%d got=%d\n", id, got);
    // falls off the end -> completes -> Async_Resume frees the slot
}

main()
{
    g_h1 = heapspace();
    printf("active start=%d\n", Async_ActiveCount());
    for (new n = 1; n <= 3; n++)
    {
        new t = Async_Start(Life, n);
        printf("started id=%d token=%d active=%d\n", n, t, Async_ActiveCount());
        Async_Resume(t, n * 100);
        printf("completed id=%d active=%d\n", n, Async_ActiveCount());
    }
    {
        new t4 = Async_Start(Life, 4);   // 4th start: reuses a freed slot (token 1 again)
        printf("reuse id=4 token=%d active=%d\n", t4, Async_ActiveCount());
        Async_Resume(t4, 400);
    }
    g_h2 = heapspace();
    printf("active end=%d heap_stable=%d\n", Async_ActiveCount(), (g_h1 == g_h2));
}
