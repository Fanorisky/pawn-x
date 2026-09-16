#include <console>

Nest2(const fmat[], ...)
{
	printf(fmat, ___(1));
}

Nest1(const outer[], ...)
{
	#pragma unused outer
	Nest2("nested: %s %s\n", ___(1));
}

main()
{
	Nest1("x", "first", "second");
}
