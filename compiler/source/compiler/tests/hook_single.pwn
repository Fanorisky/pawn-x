#include <console>
#include <hook>

hook OnFoo(a, b)
{
	printf("sum %d\n", a + b);
	return HOOK_CONTINUE;
}

main()
{
	OnFoo(4, 5);      // one hook behaves like an ordinary callback
	printf("done\n");
}
