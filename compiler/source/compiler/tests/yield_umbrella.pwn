#include <console>
#include <pawn-x>

iterfunc Count(n)
{
	for (new i = 0; i != n; ++i)
		yield return i;
}

main()
{
	foreach (new v : Count(3)) printf("u %d\n", v);
	printf("done\n");
}
