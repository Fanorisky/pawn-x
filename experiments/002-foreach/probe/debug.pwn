#include <console>
new data[32];
new count;
add(value)
{
	if (value < 0 || value >= 32) return 0;
	new i = 0;
	while (i < count && data[i] < value) i++;
	if (i < count && data[i] == value) return 1;
	i++;
	while (i <= count) { data[i] = data[i - 1]; i++; }
	data[i - 1] = value;
	count++;
	return 1;
}
main()
{
	printf("before add42: count=%d\n", count);
	add(42);
	printf("after add42: count=%d data[0]=%d data[1]=%d\n", count, data[0], data[1]);
	add(7);
	printf("after add7: count=%d data[0]=%d data[1]=%d data[2]=%d\n", count, data[0], data[1], data[2]);
}
