#include <console>
#include <foreach>

// A "foreach" over a PLAIN SET may appear directly inside a coroutine generator
// and its body may "yield" (the err-099 sugar). The loop variable and the walk
// state (the set base and the current-member cursor) are lifted into the
// generator's state block, so they survive the suspend; nothing is parked on
// the stack. The advance is value-based (setnext / setprev), so it is
// removal-safe and works for Reverse() too.

new g_set[16];

// natural syntax: yield each member scaled
iterfunc Scaled()
{
	foreach (new v : g_set)
		yield return v * 10;
}

// a lifted scalar local coexists with the loop, plus a yield before and after
iterfunc Mixed()
{
	new base = 1000;
	yield return base;
	foreach (new v : g_set)
	{
		new doubled = v + v;
		yield return base + doubled;
	}
	yield return 9999;
}

// reverse walk, and break/continue inside the loop
iterfunc RevFiltered()
{
	foreach (new v : Reverse(g_set))
	{
		if (v == 3) continue;
		yield return v;
	}
}

main()
{
	setinit(g_set);
	setadd(g_set, 3);
	setadd(g_set, 7);
	setadd(g_set, 1);          // stored ascending: 1, 3, 7

	new s;

	s = 0;
	foreach (new a : Scaled()) s += a;
	printf("scaled=%d\n", s);   // (1+3+7)*10 = 110

	s = 0;
	foreach (new b : Mixed()) s += b;
	printf("mixed=%d\n", s);    // 1000 + (1002+1006+1014) + 9999 = 14021

	new order[3], n = 0;
	foreach (new c : RevFiltered()) { order[n] = c; n++; }
	printf("rev=%d,%d n=%d\n", order[0], order[1], n);  // 7,1 n=2
}
