#include <console>
#include <hook>

// A callback whose name is long enough that the readable hidden body name
// "_hook.<callback>.<seq>" would overrun the symbol-name limit. The compiler
// falls back to a hashed hidden name so the hook still compiles and chains,
// instead of emitting a truncation warning and orphaning the body (the old
// behaviour, which cascaded into errors 055/010). YSI needs a manual
// DEFINE_HOOK_REPLACEMENT to shorten such names; pawn-x handles it silently.

hook:10 OnPlayerSelectedTheExtraordinarilyLongMenuRowCallbackName(playerid, row)
{
	printf("hookA p=%d row=%d\n", playerid, row);
	return HOOK_CONTINUE;
}

hook OnPlayerSelectedTheExtraordinarilyLongMenuRowCallbackName(playerid, row)
{
	printf("hookB p=%d row=%d\n", playerid, row);
	return HOOK_CONTINUE;
}

public OnPlayerSelectedTheExtraordinarilyLongMenuRowCallbackName(playerid, row)
{
	printf("tail p=%d row=%d\n", playerid, row);
	return 1;
}

main()
{
	OnPlayerSelectedTheExtraordinarilyLongMenuRowCallbackName(5, 42);
}
