#include <console>
#include <hook>

// A hooked callback may carry a return TAG ("hook bool:Name(...)"), exactly
// like an ordinary public. The tag lives on the synthesised dispatcher (the
// public the host calls), so a caller reading the result type-checks cleanly;
// the hook bodies stay untagged because they return the control sentinels
// (HOOK_CONTINUE / HOOK_STOP). Two hooks chain by priority, then the OR-default
// combine yields the result. YSI recognises the same tag prefixes; before this
// pawn-x rejected "hook bool:Name" outright with error 020.

hook:5 bool:OnCheckAccess(playerid)
{
	printf("A %d\n", playerid);
	return HOOK_CONTINUE;
}

hook bool:OnCheckAccess(playerid)
{
	printf("B %d\n", playerid);
	return HOOK_CONTINUE;
}

main()
{
	new bool:granted = OnCheckAccess(7);
	printf("granted=%d\n", _:granted);
}
