#include <console>
#include <async>

// LEAF await inside a FIXED-ARITY call's argument list. Reverse-order argument
// emission pushes sibling arguments before the await arg at run time, so they are
// live across the suspend. The runtime-exact operand spill copies them, and the B
// reserve is bounded by the enclosing callees' parameter footprint (getcallargbound).
// Supported for fixed-arity callees in any argument position, including nested calls.
// (Await inside a VARIADIC call, or two awaits in one expression, are rejected --
// see async_callarg_variadic_reject / async_multiawait_reject.)

foo(a, b, c) { return a*100 + b*10 + c; }
add2(a, b) { return a + b; }

async First()
{
    new p = 2, q = 3;
    new x = foo(await 0, p, q);         // await->5 : 5*100 + 2*10 + 3 = 523
    printf("first=%d\n", x);
    return x;
}

async Middle()
{
    new p = 7;
    new y = foo(p, await 0, 9);         // await->5 : 7*100 + 5*10 + 9 = 759
    printf("middle=%d\n", y);
    return y;
}

async Nested()
{
    new p = 2;
    new z = foo(add2(await 0, p), 4, 6);   // await->5: add2(5,2)=7; foo(7,4,6)=746
    printf("nested=%d\n", z);
    return z;
}

main()
{
    new t;
    t = Async_Start(First);  Async_Resume(t, 5);
    t = Async_Start(Middle); Async_Resume(t, 5);
    t = Async_Start(Nested); Async_Resume(t, 5);
    printf("active=%d\n", Async_ActiveCount());
}
