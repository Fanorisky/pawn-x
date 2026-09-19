#include <console>
#include <foreach>

new data[8];

main()
{
	setinit(data);
	setadd(data, 5);
	setadd(data, 5);
	foreach (new i : data)
	{
		printf("val %d\n", i);
	}
	printf("count=%d\n", setlen(data));
}
