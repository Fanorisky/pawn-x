#include <console>
#include <pawn-x>

/* The <pawn-x> umbrella pulls in <hook>, so the call-site hook feature
 * (hook function / hook native + continue) is reachable via the single
 * umbrella include, with no direct <hook> include. */

Val(x)
{
    return x;
}

hook function Val(x)
{
    return continue(x) + 7;              // continue -> original Val, then +7
}

main()
{
    printf("fn %d\n", Val(3));           // continue(3)=3, +7 => 10
    printf("nat %d\n", max(4, 9));       // forward-ref native hook below
}

hook native max(a, b)
{
    return continue(a, b) + 100;         // continue -> real max native, then +100
}
