#include <console>
#include <async>

// COMPOSED await MID-EXPRESSION: "base + await asyncFn()". The ergonomic compose
// path buffers hidden stack cells (inner args + the inner-B temp) for the fresh
// inner start, then FREES them before the suspend -- so at the suspend point only
// the enclosing operator temporaries are live, exactly like a leaf mid-expression
// await. Those temporaries are spilled into B before the suspend and restored on
// resume (the same runtime-exact cell copy the leaf path uses). This previously
// rejected with error 099; now it composes and the operator operand survives.

async Inner(x)
{
    new bump = await Async_Pending();     // external suspension: main resumes it
    return x + bump;                      // return-to-awaiter: resumes Outer
}

async Outer(base)
{
    // base=5, Inner(5) resumes with bump=100 -> returns 105; 5 + 105 = 110.
    new r = base + await Inner(base);
    printf("r=%d\n", r);
    return r;
}

async TwoTemp(a, b)
{
    // two live operator temps across a composed suspend: a + b + await Inner(a)
    // a=3,b=7, Inner(3) -> 3+100=103; 3 + 7 + 103 = 113.
    new r = a + b + await Inner(a);
    printf("two=%d\n", r);
    return r;
}

// NOTE: two awaits in ONE expression ("await Inner(1) + await Inner(2)") are rejected
// -- at most one await per statement (see async_multiawait_reject).

main()
{
    Async_Start(Outer, 5);
    Async_ResumeInner(100);               // complete Inner -> resumes Outer mid-expr

    Async_Start(TwoTemp, 3, 7);
    Async_ResumeInner(100);

    printf("active=%d\n", Async_ActiveCount());
}
