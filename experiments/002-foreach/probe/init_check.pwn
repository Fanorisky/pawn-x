#include <console>
new g[4];
new g2;
main()
{
	printf("g[0]=%d g[1]=%d g[2]=%d g[3]=%d g2=%d\n", g[0], g[1], g[2], g[3], g2);
	g[0]=5;
	printf("after: g[1]=%d g[2]=%d g[3]=%d\n", g[1], g[2], g[3]);
}
