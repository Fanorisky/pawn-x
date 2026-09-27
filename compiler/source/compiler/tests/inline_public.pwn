#include <console>

// "using public Name<sig>": a plain public function passed as a Callback: value
// (no closure), callable through the same indirect-call path as an inline. The
// <sig> is parsed and ignored (the indirect call is positional/unchecked). A
// receiver taking Callback: thus accepts either an inline or a public.

forward Handler(v);
public Handler(v) { printf("public got %d\n", v); }

Apply(Callback:cb) { cb(11); cb(22); }

main()
{
    Apply(using public Handler<i>);
    new base = 100;
    inline Loc(v) { printf("inline got %d base=%d\n", v, base); }
    Apply(using inline Loc);
}
