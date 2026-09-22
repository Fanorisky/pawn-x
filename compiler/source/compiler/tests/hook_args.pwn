#include <console>
#include <hook>

hook OnE(&x, a[])
{
	x += 10;                        // write through the forwarded reference
	printf("A x=%d a0=%d\n", x, a[0]);
	return HOOK_CONTINUE;
}

hook OnE(&x, a[])
{
	x += a[1];                      // reference change from hook A is visible here
	printf("B x=%d a1=%d\n", x, a[1]);
	return HOOK_CONTINUE;
}

main()
{
	new v = 5;
	new arr[2] = {100, 20};
	OnE(v, arr);                    // both hooks run; reference + array forwarded
	printf("v=%d\n", v);            // v reflects the whole chain's writes
}
