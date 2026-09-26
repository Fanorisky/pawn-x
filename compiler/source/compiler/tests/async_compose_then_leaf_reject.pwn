#include <console>
#include <async>

// The ONE multi-await shape still rejected (error 099): a LEAF await that must carry
// a COMPOSED await's result across its own suspend -- composed-THEN-leaf in one
// expression. A composed await returns through the awaiter call.pri chain, and a
// leaf suspend that follows it (with the composed result live on the operand stack)
// does not unwind back to a resumable point, so the coroutine would hang. Every other
// multi-await shape works (see async_multiawait). Reorder so the leaf comes first
// ("await Leaf() + await Inner()"), or split into separate statements.

async Inner(x) { new v = await Async_Pending(); return x + v; }

async Bad()
{
    new r = await Inner(10) + await Async_Pending();   // error 099: composed-then-leaf
    printf("unreachable: %d", r);
    return r;
}

main() { Async_Start(Bad); }
