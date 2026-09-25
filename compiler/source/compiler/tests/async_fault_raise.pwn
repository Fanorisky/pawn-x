#include <console>
#include <async>

// Fault AUTO-RAISE across a composed await (experiment 012 phase 3, task_set_error
// parity): an inner "async" fn calls Async_Fail(err) then returns, and the
// awaiting Outer's "await Inner()" observes the fault AUTOMATICALLY via
// Async_Failed()/Async_Error() -- WITHOUT Inner delivering it as a normal value
// and WITHOUT Outer re-checking a leaf. Inner's leaf completes normally (300);
// Inner then chooses to raise 301 to its awaiter.

async Inner()
{
    new x = await Async_Pending();     // leaf: completed NORMALLY by main with 300
    Async_Fail(x + 1);                 // ...but Inner decides to fault its awaiter
    return -1;                         // return -> AUTO-RAISE 301 into Outer
}

async Outer()
{
    new r = await Inner();             // composed: Inner raised -> this await faults
    if (Async_Failed())
    {
        printf("outer caught auto-raised err=%d\n", Async_Error());   // 301
        return 0;
    }
    printf("outer got r=%d (no fault)\n", r);
    return r;
}

main()
{
    Async_Start(Outer);                // Outer awaits Inner, which self-parks
    printf("(awaiting)\n");
    Async_Resume(g_asyncPending, 300); // complete Inner's leaf normally with 300
    printf("active=%d\n", Async_ActiveCount());
}
