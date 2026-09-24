#include <console>
#include <foreach>

// Fib yields successive Fibonacci numbers; (a, b) are loop-carried locals
// that must survive across each yield.
iterfunc Fib()
{
	new a = 0, b = 1;
	for (new k = 0; k != 8; ++k)
	{
		yield return a;
		new t = a + b;
		a = b;
		b = t;
	}
}

main()
{
	foreach (new v : Fib()) printf("f %d\n", v);   // 0 1 1 2 3 5 8 13
	printf("done\n");
}
