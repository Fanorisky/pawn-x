#include <console>
#include <async>

// More than one "await" in ONE statement is rejected (error 099). Two suspends in a
// single expression do not sequence correctly through the shared operand path -- the
// earlier result is lost and the coroutine leaks. This was a silent miscompile; it
// now rejects cleanly, for every mix of leaf and composed awaits (including
// "await asyncFn() + await asyncFn()"). Split into separate statements:
// "new a = await A(); new b = await B(); r = a + b;". (A SINGLE await -- mid-expression,
// as one fixed-arity call argument, composed, or in a loop -- is fully supported;
// and separate for-header clauses may each carry one await, see
// async_await_for_clauses.)

Awaitable() { return 0; }          // a leaf awaitable (returns a token)

async T()
{
    new r = await Awaitable() + await Awaitable();   // error 099: two awaits, one expr
    printf("r=%d\n", r);
    return r;
}

main() { new t = Async_Start(T); Async_Resume(t, 4); Async_Resume(t, 5); }
