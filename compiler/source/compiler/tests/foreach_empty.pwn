#include <console>

new data[8];

main()
{
	Iter_Init(data, 8);
	foreach (new i : data)
	{
		printf("val %d\n", i);
	}
	printf("done\n");
}
