#include <console>

new data[8];

main()
{
	Iter_Init(data, 8);
	Iter_Add(data, 9);
	printf("c9=%d c8=%d\n", Iter_Contains(data, 9), Iter_Contains(data, 8));
	Iter_Add(data, 12);
	printf("count=%d\n", Iter_Count(data));
}
