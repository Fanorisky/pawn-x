#include <console>
#include <async>

// A "for" header may contain an "await" in its condition AND in its increment
// (and/or init): those are independent operand-stack expressions evaluated at
// different points in the loop, so they do NOT count as "two awaits in one
// expression" (which is rejected -- see async_multiawait_reject). pc_awaitseq is
// reset per test-condition and per doexpr comma-clause, so a for-header's cond and
// incr awaits each start fresh. (A single clause with two awaits, e.g. an incr of
// "i += await A() + await B()", still rejects.)

async DualClause()
{
    new hits = 0;
    for (new i = 0; (await 0) != 0; i += (await 0))   // await in cond AND incr
        hits++;
    printf("hits=%d\n", hits);
    return hits;
}

main()
{
    new t = Async_Start(DualClause);
    Async_Resume(t, 1);   // cond -> nonzero, enter body
    Async_Resume(t, 5);   // incr
    Async_Resume(t, 1);   // cond -> nonzero, enter body
    Async_Resume(t, 5);   // incr
    Async_Resume(t, 0);   // cond -> zero, exit
    printf("active=%d\n", Async_ActiveCount());
}
