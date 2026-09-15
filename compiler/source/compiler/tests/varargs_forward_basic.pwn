#include <console>

MyPrintf(const fmat[], ...)
{
	printf(fmat, ___);
}

main()
{
	MyPrintf("basic: %d %d\n", 42, 43);
	MyPrintf("basic: %s\n", "hello");
	MyPrintf("basic: none\n");
}
