#include <console>
#include <async>

// Combinators (experiment 012 phase 3): task_all / task_any parity, native and
// plugin-free, built purely over the self-park / Async_Resume seam (no compiler
// change). A coroutine awaits a GATE that fans several outstanding operations in:
//   ALL -> resume once every operation completes; value = SUM of their results.
//   ANY -> resume on the FIRST completion; value = that result; rest ignored.
// A synthetic pump reports completions with Async_GateFeed(gate, value), exactly
// where a real host adapter (timer/DB/HTTP) would. Async_LastGate() hands the
// pump the id of the gate the coroutine just created.

async WaitAll()
{
    new g = Async_All(3);                 // resume once ALL 3 operations complete
    new total = await Async_Wait(g);      // park; total = 10+20+30 = 60
    printf("all done total=%d\n", total);
    return total;
}

async WaitAny()
{
    new g = Async_Any(3);                 // resume on the FIRST of 3
    new first = await Async_Wait(g);      // park; first = 77
    printf("any done first=%d\n", first);
    return first;
}

main()
{
    // --- ALL: three feeds; the third meets the policy and resumes with the sum.
    Async_Start(WaitAll);
    new gAll = Async_LastGate();
    printf("(all parked)\n");
    Async_GateFeed(gAll, 10);
    Async_GateFeed(gAll, 20);
    Async_GateFeed(gAll, 30);             // 3rd feed -> resume WaitAll with 60

    // --- ANY: the first feed resumes; the later feed is ignored (already fired).
    Async_Start(WaitAny);
    new gAny = Async_LastGate();
    printf("(any parked)\n");
    Async_GateFeed(gAny, 77);             // first feed -> resume WaitAny with 77
    Async_GateFeed(gAny, 88);             // ignored: policy already delivered

    // Both coroutines completed and both gates fired -> everything back to baseline.
    printf("active=%d gates=%d\n", Async_ActiveCount(), Async_GateActive());
}
