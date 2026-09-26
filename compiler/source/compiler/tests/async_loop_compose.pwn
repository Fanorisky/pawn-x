#include <console>
#include <async>

// COMPOSED "await asyncFn()" inside a loop. A leaf await in a loop already worked
// (async_loop); the composed form was once rejected (error 269) as a conservative
// guard after the "spurious completion after one iteration" miscompile. That root
// cause was the B[0]-unchanged completion heuristic in doasyncresume, since fixed
// to test PRI==generator_iterstop -- which repairs the composed loop too: each
// iteration starts the inner from the arena, frees it on completion, and the hidden
// inner-start cells are freed within the iteration, so the stack balances across the
// back-edge. This exercises for/while/do-while, statement- and mid-expression
// position, break/continue/conditional, and arena reuse past ASYNC_MAX (16).

async Inner(x)
{
    new v = await Async_Pending();
    return x + v;                     // return-to-awaiter: resumes the loop body
}

async ForStmt()                        // statement-position compose, for-loop
{
    new total = 0;
    for (new k = 1; k <= 3; k++)
    {
        new r = await Inner(k);
        total += r;
    }
    printf("for=%d\n", total);         // (1+10)+(2+10)+(3+10) = 36
    return total;
}

async ForMid()                         // mid-expression compose, for-loop
{
    new total = 0;
    for (new k = 1; k <= 3; k++)
        total += await Inner(k);
    printf("mid=%d\n", total);         // 36
    return total;
}

async Big()                            // 20 > ASYNC_MAX (16): inner B freed + reused
{
    new total = 0;
    for (new k = 0; k < 20; k++)
        total += await Inner(1);       // 20 * (1+5) = 120
    printf("big=%d\n", total);
    return total;
}

async BreakCont()                      // break + continue around a composed suspend
{
    new total = 0;
    for (new k = 0; k < 10; k++)
    {
        if (k == 2) continue;          // skip one composed await
        if (k == 4) break;             // exit after k=0,1,3
        total += await Inner(k);
    }
    printf("bc=%d\n", total);          // k=0,1,3 -> (0+1)+(1+1)+(3+1) = 7
    return total;
}

main()
{
    Async_Start(ForStmt);   for (new i = 0; i < 3; i++) Async_ResumeInner(10);
    Async_Start(ForMid);    for (new i = 0; i < 3; i++) Async_ResumeInner(10);
    Async_Start(Big);       for (new i = 0; i < 20; i++) Async_ResumeInner(5);
    Async_Start(BreakCont); for (new i = 0; i < 3; i++) Async_ResumeInner(1);
    printf("active=%d\n", Async_ActiveCount());
}
