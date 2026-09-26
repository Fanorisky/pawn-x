#include <console>
#include <async>

// MULTIPLE awaits in one statement. "await" binds at UNARY precedence, so
// "await A() + await B()" is "(await A()) + (await B())" -- two independent suspends
// the enclosing operator combines, each spilling its live operand temporaries across
// its own suspend. Works for leaf+leaf, three-in-a-row, an await in a ternary
// condition, composed+composed, and leaf-then-composed. (The one rejected shape,
// composed-THEN-leaf in one expression, is async_compose_then_leaf_reject.)

Awaitable() { return 0; }                         // a leaf awaitable (returns a token)
async Inner(x) { new v = await Async_Pending(); return x + v; }

async LeafLeaf()       { new r = await Awaitable() + await Awaitable();            printf("leafleaf=%d\n", r); return r; }
async ThreeAw()        { new r = await Awaitable() + await Awaitable() + await Awaitable(); printf("three=%d\n", r); return r; }
async Ternary()        { new r = (await Awaitable()) ? 111 : 222;                 printf("tern=%d\n", r); return r; }
async ComposeCompose() { new r = await Inner(1) + await Inner(2);                 printf("compcomp=%d\n", r); return r; }
async LeafCompose()    { new r = await Awaitable() + await Inner(10);             printf("leafcomp=%d\n", r); return r; }

main()
{
    new t;
    t = Async_Start(LeafLeaf);   Async_Resume(t, 4); Async_Resume(t, 5);          // 4+5 = 9
    t = Async_Start(ThreeAw);    Async_Resume(t, 1); Async_Resume(t, 2); Async_Resume(t, 3); // 1+2+3 = 6
    t = Async_Start(Ternary);    Async_Resume(t, 1);                              // true -> 111
    Async_Start(ComposeCompose); Async_ResumeInner(100); Async_ResumeInner(100);  // 101+102 = 203
    t = Async_Start(LeafCompose); Async_Resume(t, 3); Async_ResumeInner(4);       // 3 + (10+4) = 17
    printf("active=%d\n", Async_ActiveCount());
}
