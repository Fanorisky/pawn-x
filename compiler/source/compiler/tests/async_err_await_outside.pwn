#include <console>
#include <async>

/* "await" is the async coroutine's suspend point; it is only meaningful inside
 * an "async" function (which has a state block B for the scheduler to resume).
 * Using it in an ordinary function must be rejected with a dedicated error. */
NotAsync()
{
    new v = await 0;
    printf("%d\n", v);
}

main() { NotAsync(); }
