#include <console>
#include <foreach>

new s[16];

main()
{
	setinit(s);
	printf("e_first=%d\n", setfirst(s));   // -1 (empty)
	printf("e_last=%d\n", setlast(s));     // -1 (empty)
	setadd(s, 30); setadd(s, 10); setadd(s, 20);
	printf("first=%d\n", setfirst(s));     // 10 (smallest)
	printf("last=%d\n", setlast(s));       // 30 (largest)
}
