#include <console>

new data[8];

main()
{
	Iter_Init(data, 8);
	Iter_Add(data, 42);
	Iter_Add(data, 7);
	foreach (new i : data)
	{
		printf("val %d\n", i);
	}
}
