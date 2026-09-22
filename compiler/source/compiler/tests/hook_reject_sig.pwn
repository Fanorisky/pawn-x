#include <console>
#include <hook>

hook OnFoo(a)
{
	printf("A %d\n", a);
	return HOOK_CONTINUE;
}

hook OnFoo(a, b)
{
	printf("B %d\n", a + b);
	return HOOK_CONTINUE;
}

main()
{
	OnFoo(1);
}
