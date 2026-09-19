#include <console>
#include <foreach>

new data[8];

main()
{
	setinit(data);
	foreach (new i : data)
	{
		printf("val %d\n", i);
	}
	printf("done\n");
}
