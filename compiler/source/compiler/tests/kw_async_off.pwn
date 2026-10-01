#include <console>

// Native-async keywords are opt-in: they turn on only when <async> is included
// (which emits #pragma pawnx_async). With <async> NOT included, "async" and
// "await" are ordinary identifiers, so they may name a variable or a parameter
// and PawnPlus's own await/yield can coexist with pawn-x.
foo(await) return await * 2;

main()
{
	new async = 21;
	printf("%d\n", foo(async));
}
