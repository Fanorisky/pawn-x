#include <open.mp>
#include <async_omp>

// Proof of the GAP: the SAME two-suspends-in-one-expression that PawnPlus runs fine
// (see combo_gap.pwn) is REJECTED by the pawn-x native compiler with error 099 --
// pawn-x lifts only the coroutine's own locals, so it cannot carry the first await's
// result across the second suspend. Compile this with build/pawncc to see the reject.
// Workaround (native): split into separate statements, as NX_Work does in combo_gap.

async Bad()
{
    new r = await Async_Ms(150) + await Async_Ms(150);   // error 099 (two awaits, one expr)
    printf("unreachable: %d", r);
    return r;
}

public OnGameModeInit() { Async_Start(Bad); return 1; }
main() {}
