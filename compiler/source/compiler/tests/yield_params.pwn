#include <console>
#include <foreach>

// A generator that takes real user parameters (step, target). "step" and
// "target" must be copied from the incoming stack into the state block on the
// FRESH call and survive every "yield" suspend so the loop advances correctly.
// Because Steps has parameters, its generator-ness is only discoverable once
// "yield" is seen, so this is the first test to exercise the yield-triggered
// "sc_reparse" addressing pass.
iterfunc Steps(step, target)
{
	new total = 0;
	while (total < target)
	{
		yield return total;
		total += step;
	}
}

main()
{
	foreach (new v : Steps(10, 100)) printf("s %d\n", v);   // 0 10 20 ... 90
	printf("done\n");
}
