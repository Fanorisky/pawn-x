#include <console>
#include <async>

/* "await" inside a loop body is rejected (error 269). An "async" coroutine has a
 * SINGLE resume landing (its saved CIP); a loop back-edge would jump over that
 * suspend on the second iteration and the completion detection misfires -- the
 * coroutine would silently run one iteration then spuriously complete. The
 * compiler rejects it cleanly rather than miscompile. (Lift the loop out of the
 * coroutine, or drive the iteration from whatever resumes the coroutine.) */
async BadLoop()
{
    new total = 0;
    for (new i = 0; i < 3; i++)
    {
        new r = await 0;              /* error 269: await inside a loop */
        total += r;
    }
    printf("%d\n", total);
}

main() { Async_Start(BadLoop); }
