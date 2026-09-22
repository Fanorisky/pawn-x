// dynhook pre-hook + chain-control proof (completes Phase B).
// Handlers now run BEFORE the original and can cancel/replace it via the same
// control values as the compiler `hook`: HOOK_STOP (-1) cancels and the callback
// returns 0, HOOK_STOP_1 (-2) cancels and returns 1, HOOK_CONTINUE (1/0) falls
// through to the next handler and finally the original.

#include <open.mp>
#include <dynhook>
#include <hook>        // HOOK_CONTINUE / HOOK_STOP / HOOK_STOP_1 constants

forward OnAct(a);
public OnAct(a) { printf("  ORIGINAL OnAct a=%d", a); return 1; }

forward PreLog(a);
forward Veto(a);
public PreLog(a) { printf("  pre PreLog a=%d", a); return HOOK_CONTINUE; }
public Veto(a)   { printf("  pre Veto a=%d CANCEL", a); return HOOK_STOP; }

main()
{
	dynhook_intercept("OnAct");

	print("[C] t0 no handlers -> original only");
	CallLocalFunction("OnAct", "i", 1);

	dynhook_add("OnAct", "PreLog");
	print("[C] t1 PreLog (pre) then original");
	CallLocalFunction("OnAct", "i", 2);

	dynhook_add("OnAct", "Veto");
	print("[C] t2 PreLog, then Veto cancels -> original NOT run");
	new r = CallLocalFunction("OnAct", "i", 3);
	printf("[C] returned %d (expect 0 from HOOK_STOP)", r);

	printf("[DONE] dynC");
}
