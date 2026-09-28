#include <console>
#include <hook>

// "hook default Name = N" sets the chain seed and the combine mode: default 0
// ORs the hook returns (a command chain: 0 unless a hook claims it), default 1
// ANDs them (a confirm chain). Here default 0, so the chain stays 0 until a
// hook returns 1. This is the seed, not a forced override of the chain.
hook default OnCmd = 0;

hook OnCmd(a)
{
	if (a == 1) return HOOK_CONTINUE;   // claim it -> OR -> 1
	return HOOK_CONTINUE_0;             // not mine -> leave the chain at 0
}

main()
{
	printf("handled=%d\n", OnCmd(1));     // claimed -> 1
	printf("unhandled=%d\n", OnCmd(2));   // nobody claimed -> 0
}
