#include <console>
#include <foreach>

new data[8];

main()
{
	Iter_Init(data);
	Iter_Add(data, 1);
	Iter_Add(data, 2);
	Iter_Add(data, 3);
	foreach (new i : data)
	{
		if (i == 2)
		{
			Iter_Remove(data, 2);
			break;
		}
		printf("val %d\n", i);
	}
	printf("count=%d\n", Iter_Count(data));
}
