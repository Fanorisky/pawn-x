#include <console>
#include <iterators>

main()
{
	set_foreach (new i : Powers(2, 100))   // powers of 2 that are < 100
		printf("v %d\n", i);
	printf("done\n");
}
