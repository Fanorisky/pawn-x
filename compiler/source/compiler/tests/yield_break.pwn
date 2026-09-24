#include <console>
#include <foreach>

iterfunc UpTo()
{
	for (new i = 0; i != 100; ++i)
		yield return i;
}

main()
{
	foreach (new v : UpTo())
	{
		printf("v %d\n", v);
		if (v == 2) break;        // must free the block cleanly
	}
	printf("done\n");
}
