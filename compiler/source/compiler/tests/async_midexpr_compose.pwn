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

async TwoAwait()
{
    // the HARD shape: the first composed await's RESULT is a live operator temp
    // across a SECOND composed await's suspend.  await Inner(1) -> 101 (pushed),
    // await Inner(2) -> 102, 101 + 102 = 203.
    new r = await Inner(1) + await Inner(2);
    printf("twoawait=%d\n", r);
    return r;
}

main()
{
    Async_Start(Outer, 5);
    Async_ResumeInner(100);               // complete Inner -> resumes Outer mid-expr

    Async_Start(TwoTemp, 3, 7);
    Async_ResumeInner(100);

    Async_Start(TwoAwait);
    Async_ResumeInner(100);               // Inner(1) -> 101, re-suspends Outer
    Async_ResumeInner(100);               // Inner(2) -> 102, resumes: 101+102=203

    printf("active=%d\n", Async_ActiveCount());
}
