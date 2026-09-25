#include <console>
#include <async>

/* Local array/string locals of an "async" coroutine -- 1-D (async_array_across)
 * AND multi-dimensional (async_array_multi) -- are now LIFTED into the state
 * block and survive an "await". The remaining array-across-await limit is an
 * array PASSED AS A PARAMETER: it is a pointer into the CALLER's storage, which
 * cannot be lifted into this coroutine's block and may not outlive the suspend.
 * That must still be rejected at compile time rather than silently miscompiled. */
async Bad(const grid[])
{
    new v = await grid[0];
    printf("%d\n", v);
}

main() { new a[3]; a[0] = 1; Async_Start(Bad, a); }
