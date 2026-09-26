#include <console>
#include <async>

/* SPIKE (exp 012 full-context de-risk): ARBITRARY locals -- a 1-D array and a
 * string -- written before an "await" and read after it. The array/string cells
 * are LIFTED into the coroutine's arena state block, so they LIVE there across
 * the suspend (no save/restore needed); on resume they read back intact.
 *   buf = 10,20,30,40 (survives the await); sum = 100; msg = "hello". */
AddScore(a, b) { return a + b; }

async ArrayAcross(seed)
{
    new buf[4];
    buf[0] = seed;          // 10
    buf[1] = seed * 2;      // 20
    buf[2] = seed * 3;      // 30
    buf[3] = seed * 4;      // 40
    new msg[8] = "hello";   // a lifted string too

    new got = await AddScore(seed, 0);   // suspend; pump resumes with 99

    printf("got=%d\n", got);
    new sum = buf[0] + buf[1] + buf[2] + buf[3];   // must still be 100
    printf("after: %d %d %d %d sum=%d\n", buf[0], buf[1], buf[2], buf[3], sum);
    printf("msg=%s\n", msg);
}

main()
{
    new t = Async_Start(ArrayAcross, 10);
    printf("%s\n", "(started; suspended at await)");
    Async_Resume(t, 99);
}
