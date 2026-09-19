#include <console>
#include <foreach>

new data[8];

main()
{
	setinit(data);
	setadd(data, 9);
	printf("c9=%d c8=%d\n", sethas(data, 9), sethas(data, 8));
	setadd(data, 12);
	printf("count=%d\n", setlen(data));
}
