#include <console>
#include <async>

// MID-EXPRESSION await (operand-stack spill): an "await" may appear with live
// operator temporaries on the stack (e.g. "p + await F()", "p*2 + await F()",
// "p + q + await F()", or as a non-first call argument). The live temporaries
// (the runtime region [STK, frame boundary)) are copied into the coroutine block
// B before the suspend and copied back on resume -- a runtime-exact cell copy, so
// any expression shape and any control flow reads back correctly. This previously
// SILENTLY miscompiled (the other operand was lost across the suspend).

foo(a, b, c) { return a + b + c; }

async Branch(sel)
{
    new p = 3, q = 7, r = 0;
    if (sel == 0) r = p + await 0;            // one temp:  3 + 100     = 103
    else if (sel == 1) r = p * 2 + await 0;   // one temp:  6 + 100     = 106
    else if (sel == 2) r = p + q + await 0;   // two temps: 3 + 7 + 100 = 110
    else r = foo(p, q, await 0);              // two arg temps:         = 110
    printf("sel=%d r=%d\n", sel, r);
    return r;
}

async LoopMid()
{
    new total = 0;
    for (new i = 1; i <= 3; i++)
        total = total + await 0;              // mid-expression AND loop-carried
    printf("loop total=%d\n", total);         // 10 + 20 + 30 = 60
    return total;
}

main()
{
    for (new s = 0; s < 4; s++)
    {
        new t = Async_Start(Branch, s);
        Async_Resume(t, 100);
    }
    new g = Async_Start(LoopMid);
    Async_Resume(g, 10); Async_Resume(g, 20); Async_Resume(g, 30);
    printf("active=%d\n", Async_ActiveCount());
}
