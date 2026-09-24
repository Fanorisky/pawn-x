#include <console>
#include <foreach>

iterfunc UpTo(n)
{
	for (new i = 0; i != n; ++i)
		yield return i;
}

main()
{
	foreach (new a : UpTo(3))            // outer generator instance
	{
		printf("a %d\n", a);
		foreach (new b : UpTo(2))        // inner instance over the SAME symbol
			printf("  b %d\n", b);
	}
	printf("done\n");
}
