#include <console>
#include <hook>

hook OnFoo(a)
{
	printf("A %d\n", a);
	return HOOK_STOP_1;    // halt the chain, callback returns 1
}

hook OnFoo(a)
{
	printf("B %d\n", a);   // must NOT run
	return HOOK_CONTINUE;
}

main()
{
	new r = OnFoo(5);
	printf("r=%d\n", r);
}
