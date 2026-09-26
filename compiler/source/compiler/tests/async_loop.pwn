#include <console>
#include <async>

// Loop-carried LEAF "await" (experiment 012 phase 3): an "async" coroutine may
// await inside a while/for/do loop -- loop state (counter, accumulator, arrays)
// is lifted into the coroutine block and survives every suspend, and the
// coroutine re-parks each iteration instead of spuriously completing. This works
// because doasyncresume detects completion via the iterstop sentinel in PRI, not
// the old "B[0] unchanged" heuristic (which a loop re-suspending at the SAME
// await tripped). (Composed "await asyncFn()" inside a loop is rejected, error
// 269 -- see async_loop_compose_reject.)

// 1) for-loop with a leaf await, accumulating across iterations.
async SumLoop()
{
    new total = 0;
    for (new i = 0; i < 3; i++)
    {
        new r = await 0;               // suspend each iteration; total/i survive
        total += r;
    }
    printf("sumloop total=%d\n", total);   // 10+20+30 = 60
    return total;
}

// 2) while-loop writing an ARRAY element after each await.
async WhileArr()
{
    new buf[4];
    new i = 0;
    while (i < 4)
    {
        new r = await 0;
        buf[i] = r * 2;                // array cell written after the suspend
        i++;
    }
    printf("whilearr %d %d %d %d\n", buf[0], buf[1], buf[2], buf[3]);   // 10 12 14 16
    return 0;
}

main()
{
    new t = Async_Start(SumLoop);
    Async_Resume(t, 10); Async_Resume(t, 20); Async_Resume(t, 30);

    new w = Async_Start(WhileArr);
    Async_Resume(w, 5); Async_Resume(w, 6); Async_Resume(w, 7); Async_Resume(w, 8);

    printf("active=%d\n", Async_ActiveCount());
}
