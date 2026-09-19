#include <console>
#include <foreach>

iterfunc Range(cur, lo, hi)
{
	if (cur == ITER_STOP)
		return (lo < hi) ? lo : ITER_STOP;
	if (cur + 1 < hi)
		return cur + 1;
	return ITER_STOP;
}

main()
{
	set_foreach (new i : Range(5, 5))   // empty: lo >= hi -> body never runs
	{
		printf("v %d\n", i);
	}
	printf("done\n");
}
