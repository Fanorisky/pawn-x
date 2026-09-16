#include <console>

NotVariadic(a)
{
	printf("bad: %d\n", ___);
	#pragma unused a
}

main()
{
	NotVariadic(1);
}
