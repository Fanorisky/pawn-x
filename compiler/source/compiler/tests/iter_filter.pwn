#include <console>
#include <iterators>

new s[16];

main()
{
	setinit(s);
	for (new k = 1; k <= 6; k++) setadd(s, k);
	// inline predicate: keep odd members. Proves an iterfunc can take both a set
	// (array arg) and a Callback (using inline) and drive it through foreach.
	inline IsOdd(v) { return v & 1; }
	foreach (new i : Filter(s, using inline IsOdd))
		printf("%d\n", i);   // 1 3 5
}
