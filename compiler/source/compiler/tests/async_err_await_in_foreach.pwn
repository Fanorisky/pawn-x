#include <console>
#include <async>
#include <foreach>

/* An "await" cannot appear where stack storage is live. A "foreach" allocates
 * loop/hidden cells on the stack; those are not lifted into the state block, so
 * a suspend inside the loop would rebuild the frame without them on resume. The
 * compiler must reject it rather than miscompile. */
new g_set[8];

async Bad()
{
    foreach (new x : g_set)
    {
        new got = await x;
        printf("%d\n", got);
    }
}

main()
{
    setinit(g_set);
    setadd(g_set, 3);
    Async_Start(Bad);
}
