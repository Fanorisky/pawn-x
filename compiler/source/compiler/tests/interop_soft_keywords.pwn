#include <console>
#include <async>

// Context-sensitive (soft) keywords: pawn-x's added keywords -- async, await,
// yield, hook, iterfunc, foreach -- are recognized as keywords only in their own
// statement/expression positions. In a NAME-introducing position (after a tag,
// or after new/static/stock/public/forward/native/const/operator, or after ".")
// they parse as ordinary identifiers, so pawn-x can compile third-party includes
// (e.g. PawnPlus, whose natives take a "bool:hook" parameter) that use those
// words as names in declarations -- while the keywords keep working in pawn-x's
// own code. (Using such a name as a bare operand mid-expression is still parsed
// as the keyword; declaration/parameter/tag positions are what include prototypes
// need.)

// Keyword words used as tagged PARAMETER NAMES (like PawnPlus's bool:hook):
native dummy(bool:hook, bool:async, bool:yield, bool:foreach, bool:iterfunc);

AddScore(a, b) { return a + b; }

async Work()                       // "async" + "await" still work AS KEYWORDS
{
    new base = 7;
    new s = await AddScore(base, 1);
    printf("s=%d base=%d\n", s, base);   // resume 100 -> s=100, base survived
    return s;
}

main()
{
    new t = Async_Start(Work);
    Async_Resume(t, 100);
}
