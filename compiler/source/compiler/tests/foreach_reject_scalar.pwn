#include <console>
#include <foreach>

new sets[3][8];

main()
{
	Iter_Init(sets[0]);
	Iter_Add(sets[0], 1);
	/* sets[0][1] is a single cell, not a row -> must be rejected */
	foreach (new i : sets[0][1])
	{
		printf("%d\n", i);
	}
}
