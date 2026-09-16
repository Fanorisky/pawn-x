#include <console>
/*
 * Hand-rolled reference for the compact sorted-set layout (exp 002, Task 0).
 * `new data[cap]` holds a COMPACT SORTED RUN:
 *   data[0 .. count-1] = distinct in-use values, ascending
 *   count              = number of in-use items
 * Values must be in [0, cap) — an out-of-range value is rejected.
 * Invariants (acceptance criteria for Task 2/3):
 *   I1 sorted   data strictly ascending over the compact run
 *   I2 nodup    no value appears twice
 *   I3 count    count == number of in-use items
 *   I4 addshift add inserts in sorted position, shifting tail right
 *   I5 rmtail    remove shifts tail left, count--
 */

new data[32];
new count;

add(value)
{
	if (value < 0 || value >= 32)
		return 0;
	new i = 0;
	while (i < count && data[i] < value)
		i++;
	if (i < count && data[i] == value)
		return 1;   // already present (I2)
	// shift [i..count-1] right by one cell, then insert at i
	new pos = i;
	i = count;
	while (i > pos)
	{
		data[i] = data[i - 1];
		i--;
	}
	data[pos] = value;
	count++;
	return 1;
}

remove(value)
{
	new i = 0;
	while (i < count && data[i] != value)
		i++;
	if (i >= count)
		return 0;
	i++;
	while (i < count)
	{
		data[i - 1] = data[i];
		i++;
	}
	count--;
	return 1;
}

contains(value)
{
	new i = 0;
	while (i < count && data[i] < value)
		i++;
	return i < count && data[i] == value;
}

main()
{
	add(20);
	add(7);
	add(30);
	add(7);      // duplicate -> I2, no grow
	printf("count=%d c7=%d c21=%d\n", count, contains(7), contains(21));
	new i = 0;
	while (i < count)
	{
		printf("val %d\n", data[i]);
		i++;
	}
	remove(7);
	printf("afterrm count=%d c7=%d\n", count, contains(7));
}
