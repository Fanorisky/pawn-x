#include <console>
#include <foreach>

iterfunc Nothing() { }

main()
{
	new n = 0;
	foreach (new v : Nothing()) { n++; }
	printf("empty n=%d\n", n);            // must be 0, loop must terminate
	printf("done\n");
}
