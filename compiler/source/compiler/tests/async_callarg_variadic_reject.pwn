#include <console>
#include <async>

// A leaf "await" inside a VARIADIC call's argument list (e.g. printf) is rejected
// with error 099. Reverse-order emission pushes the arguments AFTER the await before
// it at run time, so they are live across the suspend -- but the operand spill's B
// reserve is fixed at the await point, before those trailing args are parsed, and a
// variadic callee's count is not bounded by its fixed parameter list. There is no
// safe compile-time reserve, so the runtime-exact spill would overflow B and corrupt
// a neighbouring coroutine's block. Hoist the await to a statement: "new v = await
// F(); printf(\"%d\", v);". A leaf await in a FIXED-ARITY call IS supported (its
// full parameter footprint bounds the reserve -- see async_callarg).

async F()
{
    new p = 5;
    printf("%d %d\n", await 0, p);    // error 099: await inside a variadic call (printf)
    return p;
}

main() { new t = Async_Start(F); Async_Resume(t, 100); }
