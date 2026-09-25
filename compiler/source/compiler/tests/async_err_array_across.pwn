#include <console>
#include <async>

/* An array/string local of a coroutine cannot be lifted into the state block
 * (only scalars fit a slot in v1), so it would be lost across an "await"
 * suspend. Declaring one inside an "async" body must be rejected. */
async Bad()
{
    new arr[4];
    new v = await arr[0];
    printf("%d\n", v);
}

main() { Async_Start(Bad); }
