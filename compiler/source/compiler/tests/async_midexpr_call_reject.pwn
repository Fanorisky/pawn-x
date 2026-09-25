#include <console>
#include <async>

/* A leaf "await" used INSIDE a function call's argument list is rejected with
 * error 099. Pawn emits call arguments in REVERSE order, so a sibling argument
 * textually after the await is pushed BEFORE it at run time -- live across the
 * suspend but not accounted by the operand-stack spill's compile-time bound.
 * Rejected cleanly (it previously crashed or lost the siblings). Hoist the await
 * to statement position: "new v = await F(); foo(v, r);". A leaf await as an
 * operator operand ("a + await F()") IS supported. */

foo(a, b) { return a + b; }

async F()
{
    new r = 5;
    new x = foo(await 0, r);      // error 099: await inside a call-argument list
    printf("%d\n", x);
    return x;
}

main() { new t = Async_Start(F); Async_Resume(t, 100); }
