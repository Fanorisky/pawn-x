#include <console>
#include <async>

/* An array PARAMETER of an "async" coroutine is a pointer into the CALLER's
 * storage, which is gone after a suspend. It may be used before/at the first
 * await, but not after -- using it across the suspend is rejected (error 268). */
async Bad(const grid[])
{
    new v = await 0;
    printf("%d\n", grid[v]);    // grid used AFTER the suspend -> error 268
}

main() { new a[3]; a[0] = 1; Async_Start(Bad, a); }
