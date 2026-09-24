#include <console>
#include <foreach>

iterfunc Evens()
{
	for (new i = 0; i != 6; ++i)
	{
		if (i % 2) continue;      // continue across a suspend
		yield return i;
	}
}

main()
{
	foreach (new v : Evens()) printf("e %d\n", v);   // 0 2 4
	printf("done\n");
}
