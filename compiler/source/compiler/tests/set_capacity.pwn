#include <console>
#include <foreach>

// Capacity-aware set queries: capacity is the array size minus the count slot.
new s[6];   // slot 0 = count, so 5 values fit

main()
{
	setinit(s);
	printf("cap=%d\n", setcap(s));         // 5
	printf("empty=%d\n", setisempty(s));   // 1
	printf("space=%d\n", setspace(s));     // 5
	setadd(s, 10); setadd(s, 20); setadd(s, 30);
	printf("len=%d\n", setlen(s));         // 3
	printf("space=%d\n", setspace(s));     // 2
	printf("full=%d\n", setisfull(s));     // 0
	setadd(s, 40); setadd(s, 50);
	printf("full=%d\n", setisfull(s));     // 1
	printf("space=%d\n", setspace(s));     // 0
	printf("empty=%d\n", setisempty(s));   // 0
}
