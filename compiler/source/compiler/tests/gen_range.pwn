#include <console>
#include <iterators>

main()
{
	set_foreach (new i : Range(2, 6))
		printf("v %d\n", i);
	printf("done\n");
}
