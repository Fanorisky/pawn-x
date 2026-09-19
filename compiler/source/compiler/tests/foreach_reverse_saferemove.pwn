#include <console>
#include <foreach>

new data[8];

main()
{
	setinit(data);
	setadd(data, 1);
	setadd(data, 2);
	setadd(data, 3);
	setadd(data, 4);
	setadd(data, 5);
	/* removing the current element during a REVERSE walk is safe: remove
	 * shifts the tail (higher values) left, and those are already visited. */
	set_foreach (new i : Reverse(data))
	{
		printf("v %d\n", i);
		if (i % 2 == 0)
			setremove(data, i);
	}
	printf("count=%d\n", setlen(data));
}
