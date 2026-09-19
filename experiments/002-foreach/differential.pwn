#include <console>
#include <foreach>

/*
 * Differential check (spec §5): drive BOTH the native `foreach`/Iter_* set
 * AND a hand-rolled compact sorted set over the SAME add/remove sequence,
 * then assert the emitted VALUE SEQUENCES match. This proves semantic
 * equivalence (sorted, distinct, ascending), NOT layout equivalence — the
 * layouts differ by design (native: data[0]=count, data[1..count]=values;
 * hand-rolled: data[0..count-1]=values + a parallel count).
 */

// ---- native set (compiler-managed layout) ----
new nat[32];

// ---- hand-rolled reference set (probe layout) ----
new hr[32];
new hr_count;

hr_add(value)
{
	if (value < 0 || value >= 32)
		return 0;
	new i = 0;
	while (i < hr_count && hr[i] < value)
		i++;
	if (i < hr_count && hr[i] == value)
		return 1;			// already present, no duplicate
	new pos = i;
	i = hr_count;
	while (i > pos)
	{
		hr[i] = hr[i - 1];
		i--;
	}
	hr[pos] = value;
	hr_count++;
	return 1;
}

hr_remove(value)
{
	new i = 0;
	while (i < hr_count && hr[i] != value)
		i++;
	if (i >= hr_count)
		return 0;
	i++;
	while (i < hr_count)
	{
		hr[i - 1] = hr[i];
		i++;
	}
	hr_count--;
	return 1;
}

main()
{
	// Same add/remove sequence applied to BOTH sets.
	setinit(nat);
	setadd(nat, 20);
	setadd(nat, 7);
	setadd(nat, 30);
	setadd(nat, 7);			// duplicate -> no growth
	setadd(nat, 3);
	setremove(nat, 20);		// remove present
	setadd(nat, 15);
	setremove(nat, 999);		// remove absent -> no-op

	hr_add(20);
	hr_add(7);
	hr_add(30);
	hr_add(7);
	hr_add(3);
	hr_remove(20);
	hr_add(15);
	hr_remove(999);

	// Collect the native foreach value sequence.
	new nat_seq[32];
	new nat_n = 0;
	foreach (new v : nat)
	{
		nat_seq[nat_n++] = v;
	}

	// Collect the hand-rolled value sequence.
	new hr_seq[32];
	new hr_n = 0;
	new i = 0;
	while (i < hr_count)
	{
		hr_seq[hr_n++] = hr[i];
		i++;
	}

	// Print both sequences.
	printf("native  n=%d: ", nat_n);
	i = 0;
	while (i < nat_n)
	{
		printf("%d ", nat_seq[i]);
		i++;
	}
	printf("\n");

	printf("handroll n=%d: ", hr_n);
	i = 0;
	while (i < hr_n)
	{
		printf("%d ", hr_seq[i]);
		i++;
	}
	printf("\n");

	// Assert the sequences match element-for-element.
	new ok = (nat_n == hr_n);
	i = 0;
	while (ok && i < nat_n)
	{
		if (nat_seq[i] != hr_seq[i])
			ok = 0;
		i++;
	}
	printf("MATCH=%d\n", ok);
	printf("native count=%d handroll count=%d\n", setlen(nat), hr_count);
}
