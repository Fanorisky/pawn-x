#include <console>
#include <foreach>

new g_calls = 0;

step_arg()      // records that it was evaluated, returns the step (2)
{
	g_calls = g_calls + 1;
	return 2;
}

iterfunc RangeStep(cur, lo, hi, step)
{
	if (cur == ITER_STOP)
		return (lo < hi) ? lo : ITER_STOP;   // seed
	new next = cur + step;
	return (next < hi) ? next : ITER_STOP;
}

main()
{
	// the step arg is a function call: it must be evaluated ONCE (cached),
	// not once per iteration
	foreach (new i : RangeStep(0, 10, step_arg()))   // 0 2 4 6 8
	{
		printf("v %d\n", i);
	}
	printf("calls %d\n", g_calls);   // must be 1, though the loop ran 5 times
}
