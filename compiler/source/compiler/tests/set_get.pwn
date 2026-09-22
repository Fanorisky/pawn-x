#include <console>
#include <foreach>

new d[8];

main()
{
	setinit(d);
	setadd(d, 30);
	setadd(d, 10);
	setadd(d, 20);            // stored ascending: 10, 20, 30
	printf("g0=%d\n", setget(d, 0));    // 10
	printf("g1=%d\n", setget(d, 1));    // 20
	printf("g2=%d\n", setget(d, 2));    // 30
	printf("oob=%d\n", setget(d, 3));   // out of range -> -1
	printf("neg=%d\n", setget(d, -1));  // negative index -> -1
}
