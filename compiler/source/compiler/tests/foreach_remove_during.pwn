#include <console>
#include <foreach>

new data[8];

main()
{
	setinit(data);
	setadd(data, 1);
	setadd(data, 2);
	setadd(data, 3);
	set_foreach (new i : data)
	{
		if (i == 2)
		{
			setremove(data, 2);
			break;
		}
		printf("val %d\n", i);
	}
	printf("count=%d\n", setlen(data));
}
