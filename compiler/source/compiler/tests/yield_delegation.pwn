#include <console>
#include <foreach>

// yield->yield DELEGATION: a "yield" generator may iterate ANOTHER generator
// (a coroutine operand) and yield from inside that loop. The inner "foreach"
// parks its driver state (the sub-generator's state-block base, per-step arg
// cells) on the stack, which a suspend would discard -- this used to be error
// 099. The compiler now SNAPSHOTS the frame into the state block on suspend and
// restores it on resume (which lands at the same absolute stack depth), so the
// residual stack survives with no host patch. Covers a plain delegation, a
// lifted local carried across it, a tail yield after it, break mid-delegation,
// and DOUBLE-nested delegation (two snapshot levels).

iterfunc Range(n)
{
	for (new i = 0; i < n; i++)
		yield return i;
}

iterfunc DoubleRange(n)
{
	new base = 1000;                 // lifted, survives every inner yield
	foreach (new v : Range(n))
	{
		new local = v * 2;           // lifted local inside the delegated loop
		yield return base + local;
	}
	yield return 9999;               // after the delegation completes
}

iterfunc FirstTwo()
{
	new count = 0;
	foreach (new v : Range(100))
	{
		yield return v;
		count = count + 1;
		if (count >= 2) break;       // break out of the delegation mid-stream
	}
}

iterfunc Wrap()                       // DOUBLE delegation: Wrap -> FirstTwo -> Range
{
	foreach (new v : FirstTwo())
		yield return v + 50;
}

main()
{
	new sum, count;

	sum = 0; count = 0;
	foreach (new a : DoubleRange(4)) { sum += a; count++; }
	printf("dr_sum=%d dr_count=%d\n", sum, count);   // 4012 + 9999 = 14011, 5

	sum = 0;
	foreach (new b : FirstTwo()) sum += b;
	printf("ft_sum=%d\n", sum);                       // 0+1 = 1

	sum = 0;
	foreach (new c : Wrap()) sum += c;
	printf("wrap_sum=%d\n", sum);                     // (0+50)+(1+50) = 101
}
