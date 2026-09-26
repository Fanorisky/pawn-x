#include <console>
#include <async>

// A FIXED-SIZE 1-D array (or string) PARAMETER of an "async" coroutine is COPIED IN:
// its cells are copied from the caller's array into the coroutine's state block on
// the fresh call, so the coroutine owns them and they survive an "await" (the
// caller's frame is gone by then). Works for a top-level Async_Start arg and for a
// composed "await asyncFn(arr)" arg. (Unsized "arr[]", multi-dimensional, and
// by-"&"reference parameters are still rejected -- error 268 -- see
// async_array_param_reject.)

async Sum(buf[4])
{
    new before = buf[0] + buf[1] + buf[2] + buf[3];
    new bump = await Async_Pending();
    new after = buf[0] + buf[1] + buf[2] + buf[3];    // buf survived the suspend
    printf("sum before=%d after=%d bump=%d\n", before, after, bump);
    return after + bump;
}

async Greet(name[16])
{
    await Async_Pending();
    printf("hello %s\n", name);                       // string param survived the suspend
    return 1;
}

async Outer()
{
    new arr[4] = {5, 10, 15, 20};
    new r = await Sum(arr);                            // COMPOSE with an array argument
    printf("outer=%d\n", r);                           // (5+10+15+20) + 100 = 150
    return r;
}

main()
{
    new n[16] = "Ada";
    Async_Start(Greet, n); Async_ResumeInner(0);       // hello Ada
    Async_Start(Outer);    Async_ResumeInner(100);     // Sum copied-in {5,10,15,20}; outer=150
    printf("active=%d\n", Async_ActiveCount());
}
