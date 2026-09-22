#include <console>
#include <foreach>

new data[8];

main()
{
	setinit(data);
	setadd(data, 1);
	foreach (new i : 42)
	{
		printf("%d\n", i);
	}
}
