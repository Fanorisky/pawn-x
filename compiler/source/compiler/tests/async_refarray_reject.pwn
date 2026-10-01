#include <console>
#include <async>

/* A by-REFERENCE parameter of an "async" coroutine is a pointer into the
 * CALLER's storage, which is gone after the coroutine suspends. The parameter
 * may be READ before/at the first await (safe: the caller's cell is still live),
 * but USING it AFTER a suspend is a use-after-free. The compiler rejects exactly
 * that use (error 268) -- the declaration alone is allowed. */
async Bad(&x)
{
    new v = await 0;
    printf("%d\n", x + v);      // x used AFTER the suspend -> error 268
}

main() { new n = 7; Async_Start(Bad, n); }
