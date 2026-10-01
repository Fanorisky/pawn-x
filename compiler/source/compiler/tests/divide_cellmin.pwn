#include <console>

// Regression: INT_MIN / -1 overflows the signed range (UB in C, SIGFPE on
// x86). The sdiv opcodes now guard it, wrapping the quotient to cellmin with
// remainder 0 instead of crashing the whole VM. Values go through variables so
// the compiler cannot fold the division at compile time.

main()
{
	new a = cellmin, b = -1, two = 2;
	printf("half=%d\n", a / two);
	printf("hmod=%d\n", a % two);
	printf("neg=%d\n", a / b);
	printf("nmod=%d\n", a % b);
	printf("maxneg=%d\n", cellmax / b);
}
