#include <console>

new data[8];

main()
{
	Iter_Init(data, 8);
	Iter_Add(data, 1);
	foreach (new i : 42)
	{
		printf("%d\n", i);
	}
}
