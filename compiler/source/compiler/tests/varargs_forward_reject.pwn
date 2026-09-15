#include <console>

NotVariadic(a)
{
	printf("bad: %d\n", ___);
}

main()
{
	NotVariadic(1);
}
