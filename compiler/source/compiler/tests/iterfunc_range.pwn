#include <console>
#include <foreach>

iterfunc Range(cur, lo, hi)
{
	if (cur == ITER_STOP)
		return (lo < hi) ? lo : ITER_STOP;   // seed (empty if lo>=hi)
	if (cur + 1 < hi)
		return cur + 1;
	return ITER_STOP;                        // done
}

main()
{
	foreach (new i : Range(0, 5))
	{
		printf("v %d\n", i);
	}
	printf("done\n");
}
