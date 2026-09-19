#include <console>
#include <foreach>

new sets[3][8];

main()
{
	setinit(sets[0]);
	setadd(sets[0], 1);
	/* sets[0][1] is a single cell, not a row -> must be rejected */
	set_foreach (new i : sets[0][1])
	{
		printf("%d\n", i);
	}
}
