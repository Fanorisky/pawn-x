#include <console>
#include <foreach>

/* Documents the known snapshot-semantics limitation: mutating the set
 * mid-walk WITHOUT breaking is unsupported. The count is snapshotted at
 * loop entry but the array is re-read live per iteration, so an in-body
 * Iter_Remove shifts the tail left and the walk skips the value that
 * moved into the current slot while re-reading a now-stale slot. The
 * output pinned in the .meta is the ACTUAL observed behavior (a value is
 * skipped and another emitted twice), not correct iteration -- it exists
 * so this footgun cannot silently change. Do NOT mutate mid-walk. */

new data[8];

main()
{
	Iter_Init(data);
	Iter_Add(data, 1);
	Iter_Add(data, 2);
	Iter_Add(data, 3);
	Iter_Add(data, 4);
	foreach (new i : data)
	{
		if (i == 2)
			Iter_Remove(data, 2);
		printf("val %d\n", i);
	}
	printf("count=%d\n", Iter_Count(data));
}
