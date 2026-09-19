#include <console>
#include <foreach>

new d[8];

main()
{
	setinit(d);
	printf("empty=%d\n", setrandom(d));    // empty set -> -1
	setadd(d, 5);
	printf("single=%d\n", setrandom(d));   // one item -> always 5
	setadd(d, 9);
	setadd(d, 2);
	new ok = 1;
	for (new i = 0; i < 20; i++)
		if (!sethas(d, setrandom(d)))
			ok = 0;                        // every draw must be a member
	printf("allmembers=%d\n", ok);
}
