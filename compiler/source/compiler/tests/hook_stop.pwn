#include <console>
#include <hook>

hook OnFoo(a)
{
	printf("A %d\n", a);
	return HOOK_STOP;      // halt the chain, callback returns 0
}

hook OnFoo(a)
{
	printf("B %d\n", a);   // must NOT run
	return HOOK_CONTINUE;
}

main()
{
	new r = OnFoo(3);
	printf("r=%d\n", r);
}
