#include <console>
#include <hook>

// "hook default Name = N;" makes the dispatcher return N on fall-through (all
// hooks CONTINUE), instead of the last chain value. pawn-x's HOOK_RET analogue:
// e.g. a command callback defaults to 0 ("unhandled") unless a hook claims it.
hook default OnCmd = 0;

hook OnCmd(a)
{
	if (a == 1) return HOOK_STOP_1;   // "handled" -> force return 1
	return HOOK_CONTINUE;             // not handled -> let the default apply
}

main()
{
	printf("handled=%d\n", OnCmd(1));     // HOOK_STOP_1 -> 1
	printf("unhandled=%d\n", OnCmd(2));   // fall-through -> default 0 (NOT the CONTINUE value 1)
}
