#include <console>
#include <foreach>

// A "yield" generator MAY take a by-"&"reference parameter. Unlike an "async"
// coroutine (whose caller frame is freed at "await", so a reference there is
// rejected -- see async_refarray_reject), a yield generator is driven by the
// "foreach" loop, whose frame stays alive for the whole loop. So the referent
// is valid across every yield; the compiler lifts the pointer cell into the
// state block and the driver passes the argument's address. Writes through the
// reference reach the caller's variable, and reads see the current value.

iterfunc Accumulate(&total, n)
{
	for (new i = 1; i <= n; i++)
	{
		total += i;             // write through the ref, persists across yield
		yield return total;     // read the ref back
	}
}

main()
{
	new sum = 0;
	foreach (new v : Accumulate(sum, 4))
		printf("v=%d sum=%d\n", v, sum);
	printf("final=%d\n", sum);      // 1+2+3+4 = 10
}
