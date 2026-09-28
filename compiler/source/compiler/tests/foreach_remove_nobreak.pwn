#include <console>

#include <foreach>

/* Removal-safe iteration (Y-Less issue #1, now FIXED): mutating the set
 * mid-walk without breaking is supported. `foreach` iterates BY VALUE
 * (setnext), so an in-body setremove no longer skips or duplicates any
 * still-to-visit element. Removing the current value (2) here still visits
 * 3 and 4 exactly once. This was previously a documented footgun that
 * emitted `1 2 4 4`; it now emits correct `1 2 3 4`. */

new data[8];

main()
{
	setinit(data);
	setadd(data, 1);
	setadd(data, 2);
	setadd(data, 3);
	setadd(data, 4);
	foreach (new i : data)
	{
		if (i == 2)
			setremove(data, 2);
		printf("val %d\n", i);
	}
	printf("count=%d\n", setlen(data));
}
