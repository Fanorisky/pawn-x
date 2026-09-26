#include <console>
#include <async>

/* SPIKE (exp 012 full-context de-risk): an ARRAY local written before an await,
 * read after it. If the array's cells survive the suspend, buf[] still reads
 * 10,20,30,40 after the resume and the string greets correctly.
 *   Expected:  before: 10 20 30 40
 *              got=99
 *              after:  10 20 30 40  sum=100
 *              msg=hello n=5
 */
AddScore(a, b) { return a + b; }

async ArrayAcross(seed)
{
    new buf[4];
    buf[0] = seed;          // 10
    buf[1] = seed * 2;      // 20
    buf[2] = seed * 3;      // 30
    buf[3] = seed * 4;      // 40
    new msg[8] = "hello";   // a lifted string too
    printf("before: %d %d %d %d\n", buf[0], buf[1], buf[2], buf[3]);

    new got = await AddScore(seed, 0);   // suspend; pump resumes with 99

    printf("got=%d\n", got);
    new sum = buf[0] + buf[1] + buf[2] + buf[3];   // must still be 100
    printf("after:  %d %d %d %d  sum=%d\n", buf[0], buf[1], buf[2], buf[3], sum);
    printf("msg=%s\n", msg);
}

main()
{
    new t = Async_Start(ArrayAcross, 10);
    printf("%s\n", "(started; suspended at await)");
    Async_Resume(t, 99);
}
