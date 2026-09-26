#include <open.mp>
#include <async_omp>      // pawn-x NATIVE async (keyword await/async) -- no plugin
#include <PawnPlus>       // PawnPlus natives, NO PP_SYNTAX -> use raw task_*/wait_ms

// pawn-x NATIVE coroutine (compiler-emitted, keyword await)
async NX()
{
    print("[native]  start (pawn-x coroutine, no plugin)");
    await Async_Ms(200);
    new buf[8];
    for (new i = 0; i < 8; i++) buf[i] = i * i;
    await Async_Ms(200);
    new s = 0;
    for (new j = 0; j < 8; j++) s += buf[j];
    printf("[native]  done: local array survived, sum=%d", s);
    return 0;
}

public OnGameModeInit()
{
    print(">>> ONE .amx: pawn-x native async + PawnPlus, compiled by build/pawncc");
    Async_Start(NX);                 // native side: parks on its own timer
    // PawnPlus side (raw natives, no macros): wait_ms yields this public, plugin resumes
    wait_ms(150);
    print("[pawnplus] resumed after 150ms (wait_ms native)");
    new buf[8];
    for (new i = 0; i < 8; i++) buf[i] = i * i;
    wait_ms(150);
    new s = 0;
    for (new j = 0; j < 8; j++) s += buf[j];
    printf("[pawnplus] done: local array survived, sum=%d", s);
    return 1;
}
main() {}
