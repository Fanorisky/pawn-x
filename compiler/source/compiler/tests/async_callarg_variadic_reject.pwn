#include <console>
#include <async>

// A leaf "await" inside a VARIADIC call's argument list is rejected (error 099).
// Reverse-order emission makes the sibling arguments live across the suspend, and
// the operand spill's B region must be reserved for them -- but a variadic callee's
// pushed-argument count is not bounded by its fixed parameter list, so no safe
// compile-time reserve exists. Hoist the await to a statement: "new v = await F();
// printf("%d", v);". (A leaf await inside a FIXED-ARITY call IS supported -- see
// async_callarg.)

async F()
{
    new r = 5;
    printf("%d\n", await 0);    // error 099: await inside a variadic call (printf)
    return r;
}

main() { new t = Async_Start(F); Async_Resume(t, 100); }
