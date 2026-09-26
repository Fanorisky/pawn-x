#include <open.mp>
#include <async_omp>

// NATIVE (pawn-x) side of the comparison: a compiler-emitted coroutine, NO plugin.
// Same logical task as pp_demo: wait, build a LOCAL array across the wait, wait
// again, sum it. The local `buf` is LIFTED into the coroutine's state block, so it
// survives with no frame snapshot.

async NX_Work()
{
    print("[native] start (compiler coroutine, no plugin)");
    await Async_Ms(200);
    print("[native] tick @200ms");
    new buf[8];
    for (new i = 0; i < 8; i++) buf[i] = i * i;
    await Async_Ms(200);
    new sum = 0;
    for (new j = 0; j < 8; j++) sum += buf[j];
    printf("[native] done: local array survived, sum=%d", sum);   // 140
    return 0;
}

public OnGameModeInit()
{
    Async_Start(NX_Work);
    CallRemoteFunction("PP_Start", "");
    return 1;
}

main() {}
