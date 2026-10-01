#include <console>
#include <async>
#include <foreach>

/* An "await" cannot appear where stack storage is live. Iterating a PLAIN SET
 * inside an "async" is fine (its walk state is lifted into the state block --
 * see async_await_in_foreach), but iterating ANOTHER GENERATOR is not: the
 * coroutine operand parks its state-block base and per-step argument cells on
 * the stack, which are not lifted. A suspend inside the loop would rebuild the
 * frame without them on resume, so the compiler must reject it. */

iterfunc Leaf()
{
    yield return 1;
}

async Bad()
{
    foreach (new v : Leaf())
    {
        new got = await v;
        printf("%d\n", got);
    }
}

main()
{
    Async_Start(Bad);
}
