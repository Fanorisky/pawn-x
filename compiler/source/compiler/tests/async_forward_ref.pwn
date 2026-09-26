#include <console>
#include <async>

// Forward-referenced async callee (experiment 012, two-pass stability guard):
// this is async_return_to_awaiter.pwn with the declaration order INVERTED --
// Outer's ergonomic "await Inner(40)" names Inner BEFORE Inner is declared.
//
// Historically this silently miscompiled: in the single addressing pass the
// callee was not yet known (findglb NULL -> normal-await path), but the write
// pass saw the persisted uASYNC symbol and emitted the ergonomic compose path.
// The divergent instruction streams corrupted code_idx/labels -> segfault, with
// NO diagnostic. The fix forces a one-shot reparse so the addressing pass, on
// its second run, resolves Inner (its symbol persists across passes with uASYNC)
// and emits the SAME path as the write pass. Expected output is identical to the
// declared-before test: 40 + 222 = 262.

async Outer()
{
    new r = await Inner(40);          // forward ref: Inner is declared BELOW
    printf("outer got=%d\n", r);
}

async Inner(x)
{
    new bump = await Async_Pending();     // suspend; main later resumes with a value
    return x + bump;                  // return-to-awaiter: resumes Outer with this
}

main()
{
    Async_Start(Outer);               // Outer starts, awaits Inner -> parked on Inner
    printf("(outer awaiting inner)\n");
    Async_ResumeInner(222);           // complete Inner's Async_Pending with 222
    // Inner returns 40+222=262 -> Outer resumes -> prints outer got=262
}
