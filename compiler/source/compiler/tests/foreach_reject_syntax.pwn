#include <console>
#include <foreach>

new data[8];

main()
{
	Iter_Init(data);
	Iter_Add(data, 1);
	foreach (new i data)
	{
		printf("%d\n", i);
	}
}
