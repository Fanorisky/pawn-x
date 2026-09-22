#include <console>
#include <foreach>

new data[8];

main()
{
	setinit(data);
	setadd(data, 1);
	setadd(data, 2);
	setadd(data, 3);
	foreach (new i : Reverse(data))
	{
		printf("v %d\n", i);
	}
	printf("done\n");
}
