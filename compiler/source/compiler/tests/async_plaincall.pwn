#include <console>
#include <async>

// A call to an "async" function WITHOUT "await" is a fire-and-forget START. An
// async callee's prologue reads arg0 as its state block B, so the call site must
// push B as arg0 -- a bare "call" hands the callee its first user argument
// instead, and the prologue then dereferences that as B and jumps to it. Before
// the fix this printed "foo 0": the argument was consumed as B and the real
// parameter was read from the cell above it.
async Foo(x)
{
	printf("foo %d\n", x);
}

main()
{
	Foo(12345);
	printf("after-call\n");
}
