#include <console>
#include <foreach>

new d[8];

main()
{
	setinit(d);
	printf("a0=%d\n", setalloc(d));     // empty -> 0, set {0}
	printf("a1=%d\n", setalloc(d));     // -> 1, set {0,1}
	printf("a2=%d\n", setalloc(d));     // -> 2, set {0,1,2}
	setremove(d, 1);                    // set {0,2}
	printf("reuse=%d\n", setalloc(d));  // first gap is 1 -> 1, set {0,1,2}
	printf("len=%d\n", setlen(d));      // 3
	printf("g1=%d\n", setget(d, 1));    // ascending {0,1,2}, pos 1 -> 1
}
