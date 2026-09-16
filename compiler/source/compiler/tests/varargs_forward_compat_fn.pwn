#include <console>

___(x)
{
	return x + 1;
}

main()
{
	new v = ___(41);
	printf("compat fn: %d %d\n", v, ___(7));
}
