#include <console>
#include <async>

/* A 1-D array/string local of an "async" coroutine is now LIFTED into the state
 * block and survives an "await" (see async_array_across). A MULTI-DIMENSIONAL
 * array is not lifted yet, so declaring one live across an "await" must still be
 * rejected rather than silently miscompiled. */
async Bad()
{
    new grid[2][2];
    new v = await grid[0][0];
    printf("%d\n", v);
}

main() { Async_Start(Bad); }
