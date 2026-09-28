#include <console>
#include <hook>

// "hook default = 1" makes the chain AND the returns (YSI's confirm semantics):
// the chain returns 1 only if EVERY hook agrees (returns 1). Seeded at 1.
hook default OnAll = 1;

hook OnAll(a)
{
	printf("h1 %d\n", a >= 1);
	return (a >= 1) ? HOOK_CONTINUE : HOOK_CONTINUE_0;
}

hook OnAll(a)
{
	printf("h2 %d\n", a >= 2);
	return (a >= 2) ? HOOK_CONTINUE : HOOK_CONTINUE_0;
}

main()
{
	printf("all2=%d\n", OnAll(2));   // both agree -> AND -> 1
	printf("all1=%d\n", OnAll(1));   // second disagrees -> AND -> 0
}
