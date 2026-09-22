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
	foreach (new i : Reverse(Range(0, 5)))   // reverse over a generator is undefined
	{
		printf("%d\n", i);
	}
}
