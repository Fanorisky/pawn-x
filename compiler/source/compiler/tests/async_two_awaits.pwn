#include <console>
#include <async>

// Promoted from experiment 012's async_two_awaits spike, now on the finalized
// <async> PUBLIC API. Three sequential awaits in ONE body; every scalar local
// (a, b, c, and the param x) survives every suspend. A synthetic pump resumes the
// same coroutine three times in order -- its token stays valid until it finally
// completes on the third resume.
//   x = 5: a = 5+1 = 6, b = 6+10 = 16, c = 16+100 = 116,
//   final = a + b + c + x = 6 + 16 + 116 + 5 = 143.
new g_task;

// Stand-in async op (see async_basic): computes the value a real host would
// deliver through Async_Resume on completion.
AddAsync(a, b) { return a + b; }

async Chain(x)
{
    new a = await AddAsync(x, 1);      // x + 1
    new b = await AddAsync(a, 10);     // (x+1) + 10
    new c = await AddAsync(b, 100);    // (x+11) + 100
    printf("chain: a=%d b=%d c=%d final=%d\n", a, b, c, a + b + c + x);
}

main()
{
    g_task = Async_Start(Chain, 5);
    printf("%s\n", "(chain started)");
    // synthetic pump: three sequential completions, each resumes the next await.
    Async_Resume(g_task, AddAsync(5, 1));      // a = 6
    Async_Resume(g_task, AddAsync(6, 10));     // b = 16
    Async_Resume(g_task, AddAsync(16, 100));   // c = 116 -> completes, frees slot
}
