#include <console>

new data[8];

main()
{
	Iter_Init(data, 8);
	Iter_Add(data, 1);
	foreach (new i data)
	{
		printf("%d\n", i);
	}
}
