#include <console>

// two independent parked coroutines, resumed in ARBITRARY (reverse) order.
new g_h1, g_h2;

async Worker(id, seed)
{
    new local = seed * 1000;          // distinct per-invocation local
    new got = await 0;                // suspend; value delivered by scheduler
    printf("worker id=%d local=%d got=%d sum=%d\n", id, local, got, local + got + id);
}

main()
{
    __async_start(g_h1, Worker, 1, 7);
    __async_start(g_h2, Worker, 2, 9);
    print("(two workers parked)");
    // resume in reverse order with distinct values -> proves per-block state
    __async_resume(g_h2, Worker, 222);
    __async_resume(g_h1, Worker, 111);
}
