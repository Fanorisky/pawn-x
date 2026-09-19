#include <console>
#include <foreach>

new data[8];

main()
{
	setinit(data);
	setadd(data, 42);
	setadd(data, 7);
	set_foreach (new i : data)
	{
		printf("val %d\n", i);
	}
}
