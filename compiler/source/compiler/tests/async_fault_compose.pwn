#include <console>
#include <async>

// Regression (fault channel + composed await): an inner "async" fn that observes
// a LEAF fault, RECOVERS, and returns a clean value must NOT leave a stale fault
// for its awaiter. Before the fix the return-to-awaiter resume path never reset
// g_asyncFailed, so Outer's "if (Async_Failed())" wrongly tripped on Inner's
// already-handled fault. Expected: Inner handles err=42 and returns 500; Outer
// sees NO fault and r=500.

async Inner()
{
    new x = await Async_Pending();        // leaf await; main completes it as an ERROR
    if (Async_Failed())
    {
        printf("inner handled err=%d\n", Async_Error());
        return 500;                       // RECOVER: hand the awaiter a good value
    }
    return x;
}

async Outer()
{
    new r = await Inner();                // composed: Inner recovered -> r=500, no fault
    if (Async_Failed())
    {
        printf("outer WRONGLY saw fault err=%d\n", Async_Error());
        return -1;
    }
    printf("outer ok r=%d\n", r);
    return r;
}

main()
{
    Async_Start(Outer);                   // Outer awaits Inner, which self-parks
    printf("(outer awaiting inner)\n");
    Async_ResumeError(g_asyncPending, 42); // fail Inner's leaf await with code 42
    printf("active=%d\n", Async_ActiveCount());
}
