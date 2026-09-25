#include <console>
#include <async>

// Context-sensitive (soft) keywords: pawn-x's added keywords -- async, await,
// yield, hook, iterfunc, foreach -- parse as keywords only in their own positions.
//
//  * "hook", "async", "iterfunc" are DECLARATION-only keywords: after a tag
//    ("bool:hook") or a declaration specifier they are ordinary identifiers, so
//    pawn-x compiles third-party includes (PawnPlus's "bool:hook" native param).
//  * "await", "yield", "foreach" can follow a tag override or a statement label as
//    genuine keywords ("Float:await", "retry: await ...", "loop: foreach ..."), so
//    those keep working -- they downgrade only after unambiguous decl specifiers.

// declaration-only keyword words as tagged parameter names (the PawnPlus pattern):
native dummy(bool:hook, bool:async, bool:iterfunc);

AddScore(a, b) { return a + b; }

async Work()                       // "async" + "await" work AS KEYWORDS
{
    new base = 7;
    new s;
retry:                             // a statement label, then an await statement:
    s = await AddScore(base, 1);   // "await" after a label stays a KEYWORD
    printf("s=%d base=%d\n", s, base);   // resume 100 -> s=100
    return s;
}

main()
{
    new t = Async_Start(Work);
    Async_Resume(t, 100);
}
