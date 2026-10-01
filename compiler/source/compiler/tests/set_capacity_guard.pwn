#include <console>
#include <foreach>

// Regression: setadd must refuse when the set is full instead of shifting the
// tail past the end of the array (a silent out-of-bounds write). s[4] holds a
// count slot plus room for 3 values.
new s[4];

main()
{
	setinit(s);
	printf("a1=%d\n", setadd(s, 10));
	printf("a2=%d\n", setadd(s, 20));
	printf("a3=%d\n", setadd(s, 30));
	printf("a4=%d\n", setadd(s, 40));
	printf("len=%d\n", setlen(s));
	printf("full=%d\n", setisfull(s));
	printf("vals=%d,%d,%d\n", setget(s, 0), setget(s, 1), setget(s, 2));
}
