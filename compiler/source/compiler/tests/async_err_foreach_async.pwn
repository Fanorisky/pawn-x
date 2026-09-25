#include <console>
#include <async>
#include <foreach>

/* An "async" function is a coroutine driven by a scheduler via "await", not by
 * "foreach": its state block reserves a different number of head cells and uses
 * a different resume protocol than a "yield" generator. Iterating it with
 * "foreach" would under-allocate the block and drive it wrongly, so it must be
 * rejected cleanly. */
async Gen()
{
    new v = await 0;
    printf("%d\n", v);
}

main()
{
    foreach (new v : Gen())
        printf("v %d\n", v);
}
