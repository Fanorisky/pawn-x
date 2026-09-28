#include <console>
#include <hook>

// Y-Less issue: a default-0 (implicit) hook chain must OR the returns, so if ANY
// hook claims the callback (returns 1) the chain returns 1. Last-value let a
// later "not handled" hook (return 0) clobber an earlier claim, which is the
// /help bug (command runs AND the player still gets "unknown command").
hook OnCmd(a)
{
	if (a == 1) { printf("help\n"); return HOOK_CONTINUE; }   // claim
	return HOOK_CONTINUE_0;                                    // not mine
}

hook OnCmd(a)
{
	if (a == 2) { printf("commands\n"); return HOOK_CONTINUE; }   // claim
	return HOOK_CONTINUE_0;                                       // not mine
}

main()
{
	printf("c1=%d\n", OnCmd(1));   // first claims, second passes -> OR -> 1
	printf("c2=%d\n", OnCmd(2));   // first passes, second claims -> 1
	printf("c3=%d\n", OnCmd(3));   // nobody claims -> 0
}
