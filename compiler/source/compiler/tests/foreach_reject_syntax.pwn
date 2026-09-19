#include <console>
#include <foreach>

new data[8];

main()
{
	setinit(data);
	setadd(data, 1);
	set_foreach (new i data)
	{
		printf("%d\n", i);
	}
}
