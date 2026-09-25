#include <console>
#include <async>

// Return-to-awaiter + ergonomic "await asyncFn(args)" (experiment 012, task 2):
// Outer awaits Inner via the ergonomic form, which STARTS Inner and links it
// back to Outer (inner B[ASYNC_AWAITER_SLOT] = Outer's B). Inner suspends on an
// external "Async_Pending()" -- self-registered so main can resume it by token. When
// main completes Inner, Inner's "return" delivers its value STRAIGHT INTO Outer
// and resumes it: async functions compose. Expected: 40 + 222 = 262.
async Inner(x)
{
    new bump = await Async_Pending();     // suspend; main later resumes with a value
    return x + bump;                  // return-to-awaiter: resumes Outer with this
}

async Outer()
{
    new r = await Inner(40);          // ergonomic: start Inner, park until it returns
    printf("outer got=%d\n", r);
}

main()
{
    Async_Start(Outer);               // Outer starts, awaits Inner -> parked on Inner
    printf("(outer awaiting inner)\n");
    Async_ResumeInner(222);           // complete Inner's Async_Pending with 222
    // Inner returns 40+222=262 -> Outer resumes -> prints outer got=262
}
