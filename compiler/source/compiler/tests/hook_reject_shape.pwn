#include <console>
#include <hook>

hook OnY(a)
{
	printf("A %d\n", a);
	return HOOK_CONTINUE;
}

hook OnY(a[])
{
	printf("B %d\n", a[0]);   // shape mismatch: array vs plain cell
	return HOOK_CONTINUE;
}

main()
{
	OnY(5);
}
