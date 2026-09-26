#include <open.mp>
#include <async_omp>      // pawn-x NATIVE async (keyword await/async) -- no plugin
#include <PawnPlus>       // PawnPlus natives, NO PP_SYNTAX -> raw task_*/task_await

// ============================================================================
//  NATIVE (pawn-x) vs PawnPlus, SIDE BY SIDE in ONE .amx, compiled by build/pawncc
//  -- and a case PawnPlus SUPPORTS but pawn-x native does NOT.
//
//  pawn-x native suspends by lifting ONLY the coroutine's own locals into a state
//  block (no plugin). PawnPlus suspends by snapshotting the WHOLE AMX frame at
//  runtime (its plugin). That snapshot is why PawnPlus can "await" in ANY expression
//  position -- including TWO suspends in one expression -- while pawn-x native
//  requires at most one await per statement (error 099 otherwise; see gap_native_reject).
// ============================================================================

// ---- NATIVE side: the SUPPORTED shape (one await per statement) -------------
async NX_Work()
{
    print("[native]   start (pawn-x coroutine, no plugin)");
    new acc = 40;                         // a lifted local...
    await Async_Ms(150);                  // ...survives this suspend (one await/stmt)
    acc += 2;
    await Async_Ms(150);                  // ...and this one
    printf("[native]   local survived TWO awaits (separate statements): acc=%d", acc);  // 42
    // "new r = await Async_Ms(150) + await Async_Ms(150);" would be ERROR 099 here
    // -- pawn-x native cannot carry the first await's result across the second
    // suspend. See gap_native_reject.pwn for the exact rejection.
    return acc;
}

// ---- PawnPlus side: the GAP -- TWO suspends in ONE expression ---------------
DoPawnPlusGap()
{
    // task_await() = task_wait() + task_get_result(): a real suspend that PawnPlus
    // services by snapshotting the frame, so it composes inside any expression.
    new Task:a = task_new(); task_set_result_ms(a, 40, 150);   // resolves to 40 @150ms
    new Task:b = task_new(); task_set_result_ms(b, 2,  150);   // resolves to 2  @150ms
    new r = task_await(a) + task_await(b);                     // 40 + 2 = 42
    printf("[pawnplus] TWO suspends in ONE expression: r=%d  (pawn-x native: error 099)", r);
}

public OnGameModeInit()
{
    print(">>> ONE .amx (build/pawncc): pawn-x native async + PawnPlus, plus a PawnPlus-only case");
    Async_Start(NX_Work);        // native side parks on its own compiler-emitted timer
    DoPawnPlusGap();             // PawnPlus side does what native rejects
    return 1;
}
main() {}
