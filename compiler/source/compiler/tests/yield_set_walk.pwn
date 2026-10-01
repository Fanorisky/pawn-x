#include <console>
#include <foreach>

// A generator can iterate a set and yield each value TODAY, without the
// (unsupported) "yield inside a nested foreach": walk the set by VALUE with
// setfirst/setnext keeping only a lifted scalar cursor across the yield. This
// is the supported idiom for the err-099 case (no stack cursor is live).
new gSet[8];

iterfunc Walk()
{
	new v = setfirst(gSet);
	while (v != -1 && v != cellmin)
	{
		yield return v * 10;
		v = setnext(gSet, v);
	}
}

main()
{
	setinit(gSet);
	setadd(gSet, 1);
	setadd(gSet, 2);
	setadd(gSet, 3);
	new sum = 0;
	foreach (new x : Walk())
		sum += x;
	printf("sum=%d\n", sum);
}
