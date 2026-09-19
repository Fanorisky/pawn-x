#include <console>
#include <iterators>

main()
{
	set_foreach (new i : RangeStep(0, 10, 3))
		printf("v %d\n", i);
	printf("done\n");
}
