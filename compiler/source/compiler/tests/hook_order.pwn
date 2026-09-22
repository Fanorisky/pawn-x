#include <console>
#include <hook>

hook OnFoo(a)
{
	printf("A %d\n", a);
	return HOOK_CONTINUE;
}

hook OnFoo(a)
{
	printf("B %d\n", a);
	return HOOK_CONTINUE;
}

main()
{
	OnFoo(7);           // calls the generated dispatcher: both hooks, in order
	printf("done\n");
}
