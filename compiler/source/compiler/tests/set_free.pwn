#include <console>
#include <foreach>

new d[8];

main()
{
	setinit(d);
	printf("empty=%d\n", setfree(d));   // no items -> smallest free is 0
	setadd(d, 0);
	setadd(d, 1);
	setadd(d, 3);
	printf("gap=%d\n", setfree(d));     // {0,1,3} -> 2 is the first gap
	setadd(d, 2);
	printf("dense=%d\n", setfree(d));   // {0,1,2,3} -> 4
}
