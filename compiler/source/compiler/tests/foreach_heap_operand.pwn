#include <console>
#include <foreach>

new gset[8];

/* returns the compact set by value: the callee copies it onto the heap, so the
 * foreach operand is a temporary array (the case that must not leak) */
GetSet()
{
	return gset;
}

main()
{
	setinit(gset);
	setadd(gset, 5);
	setadd(gset, 2);
	setadd(gset, 8);
	/* iterate the array-returning function directly, many times: if the
	 * per-loop temporary leaked the heap would march up and eventually
	 * collide with the stack; stable output across passes proves it is freed */
	for (new pass = 0; pass < 100; pass++)
	{
		set_foreach (new i : GetSet())
		{
			if (pass == 0)
				printf("%d\n", i);
		}
	}
	printf("done\n");
}
