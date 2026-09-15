#include <console>

SkipWrapper(a, b, ...)
{
	printf("skip: %d %d %d %d\n", a, b, ___(2));
}

main()
{
	SkipWrapper(1, 2, 3, 4, 5);
	SkipWrapper(9, 8, 7);
}
