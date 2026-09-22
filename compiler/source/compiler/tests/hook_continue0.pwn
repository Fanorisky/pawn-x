#include <console>
#include <hook>

hook OnFoo(a)
{
	printf("A %d\n", a);
	return HOOK_CONTINUE_0;   // run the next hook, chain result so far 0
}

hook OnFoo(a)
{
	printf("B %d\n", a);
	return HOOK_CONTINUE_0;   // last hook continues: chain result 0
}

main()
{
	new r = OnFoo(2);
	printf("r=%d\n", r);
}
