#include <console>
#include <async>

// Two "await"s in ONE expression are rejected (error 099) when the first result must
// survive the second suspend. Two leaf suspends in a single expression do not
// sequence correctly through the shared operand path -- the earlier result is lost
// and the coroutine leaks. This was a silent miscompile; it now rejects cleanly.
// Split into separate statements: "new a = await A(); new b = await B(); r = a + b;".
// (A single await mid-expression, or a composed "await asyncFn() + await asyncFn()",
// is fine.)

Awaitable() { return 0; }          // a leaf awaitable (returns a token)

async T()
{
    new r = await Awaitable() + await Awaitable();   // error 099: two awaits, one expr
    printf("r=%d\n", r);
    return r;
}

main() { new t = Async_Start(T); Async_Resume(t, 4); Async_Resume(t, 5); }
