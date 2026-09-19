#include <console>
#include <foreach>

// a filtering generator: emits only the multiples of 3 in [lo, hi). Also
// exercises "iterfunc stock" (the generator combines with a class specifier).
iterfunc stock Mult3(cur, lo, hi)
{
	new next = (cur == ITER_STOP) ? lo : cur + 1;
	while (next < hi)
	{
		if (next % 3 == 0)
			return next;
		next = next + 1;
	}
	return ITER_STOP;
}

main()
{
	set_foreach (new i : Mult3(0, 10))   // 0 3 6 9
	{
		printf("v %d\n", i);
	}
	printf("done\n");
}
