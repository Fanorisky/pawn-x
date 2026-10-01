#include <console>

// The generator "yield" keyword turns on only when <foreach> is included (which
// emits #pragma pawnx_yield). Without it, "yield" is an ordinary identifier.
bar(yield) return yield + 1;

main()
{
	new yield = 9;
	printf("%d\n", bar(yield));
}
