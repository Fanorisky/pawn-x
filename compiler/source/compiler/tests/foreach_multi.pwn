#include <console>
#include <foreach>

new sets[3][8];   // 3 independent compact sets sharing one 2D array

main()
{
	Iter_Init(sets[0]);
	Iter_Init(sets[1]);
	Iter_Init(sets[2]);
	Iter_Add(sets[0], 42);
	Iter_Add(sets[0], 7);
	Iter_Add(sets[2], 5);
	Iter_Add(sets[2], 3);
	Iter_Add(sets[2], 9);
	/* iterate a subscripted row (the "wild" operand) */
	foreach (new i : sets[0])
	{
		printf("a %d\n", i);
	}
	/* a computed index also works, evaluated once at loop entry */
	new k = 1 + 1;
	foreach (new j : sets[k])
	{
		printf("b %d\n", j);
	}
	/* empty row: body never runs */
	foreach (new m : sets[1])
	{
		printf("c %d\n", m);
	}
	printf("done\n");
}
