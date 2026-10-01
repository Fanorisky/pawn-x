#include <console>
#include <iterators>

new s[16];

main()
{
	setinit(s);
	setadd(s, 0); setadd(s, 1); setadd(s, 2);   // 0 is a member
	foreach (new i : NonNull(s))
		printf("%d\n", i);   // 1 2 (0 skipped)
}
