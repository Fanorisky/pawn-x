#include <console>
#include <foreach>

// A generator that yields two constants, with no loop-carried locals yet.
iterfunc Pair()
{
	yield return 10;
	yield return 20;
}

main()
{
	foreach (new v : Pair()) printf("pair %d\n", v);
	printf("done\n");
}
