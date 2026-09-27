#include <console>

// Native "inline" closures, Milestone 0: an inline function is defined inside
// another function, passed to a receiver as a Callback: value via "using inline",
// and called indirectly. No closure capture yet -- the body uses only its own
// parameters and globals. Proves hoist (the body is compiled out-of-line and the
// enclosing function jumps over it), the "using inline" callback value (an entry
// address), and the receiver's indirect "cb(args)" call.pri.

Apply(Callback:cb)
{
    printf("apply start\n");
    cb(5);
    cb(10);
    printf("apply end\n");
}

Test()
{
    inline Show(v) { printf("got v=%d\n", v); }
    Apply(using inline Show);
}

main()
{
    Test();
}
