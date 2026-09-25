#include <console>
#include <async>

/* A LEAF await in a loop is supported (see async_loop), but a COMPOSED
 * "await asyncFn()" inside a loop is rejected with error 269: the compose path
 * buffers hidden stack cells (the inner's args + inner-B temp) around the suspend,
 * and re-entering that across a loop back-edge unbalances the stack. Lift the
 * composed call out of the loop, or drive the repetition from the resumer. */

async Inner(x)
{
    new v = await 0;
    return x + v;
}

async BadComposeLoop()
{
    new total = 0;
    for (new k = 0; k < 3; k++)
        total += await Inner(k);      // error 269: composed await inside a loop
    printf("%d\n", total);
    return total;
}

main() { Async_Start(BadComposeLoop); }
