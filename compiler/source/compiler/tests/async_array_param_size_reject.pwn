#include <console>
#include <async>

// Passing an array SMALLER than an async coroutine's fixed-size array parameter is
// rejected (error 047), on both the Async_Start path and a composed "await F(arr)".
// The prologue copies the parameter's DECLARED length into the coroutine block, so a
// shorter argument would be over-read from the caller's frame; the ordinary call
// path reports 047 for this and the async entry paths now do too.

async F(buf[4])
{
    new s = buf[0];
    new v = await 0;
    return s + v + buf[3];
}

main()
{
    new x[2] = {11, 22};              // 2 cells passed to a buf[4] parameter -> error 047
    new t = Async_Start(F, x);
    Async_Resume(t, 0);
}
