// pawn-x native — RUNTIME hook manipulation (the area exp 005 marked a gap).
// Compiled with the pawn-x pawncc + the `dynhook` companion plugin.
// Scenario: a game event whose handler set changes WHILE THE SERVER RUNS —
// enable admin logging, add metrics, then disable admin logging, all at runtime,
// without recompiling and without the event wiring anything itself.

#include <open.mp>
#include <dynhook>

forward OnEvent(a);
public OnEvent(a) { printf("  base OnEvent a=%d", a); return 1; }

// handlers toggled at runtime — plain publics, NOT wired as hooks anywhere
forward AdminLog(a);
forward Metrics(a);
public AdminLog(a) { printf("  [rt] AdminLog a=%d", a); return 1; }
public Metrics(a)  { printf("  [rt] Metrics a=%d", a); return 1; }

main()
{
	dynhook_intercept("OnEvent");   // transparent: chain fires when OnEvent runs

	print("[N] t0 base only");
	CallLocalFunction("OnEvent", "i", 1);

	dynhook_add("OnEvent", "AdminLog");
	print("[N] t1 +AdminLog (runtime)");
	CallLocalFunction("OnEvent", "i", 2);

	dynhook_add("OnEvent", "Metrics");
	print("[N] t2 +Metrics (runtime)");
	CallLocalFunction("OnEvent", "i", 3);

	dynhook_remove("OnEvent", "AdminLog");
	print("[N] t3 -AdminLog (runtime)");
	CallLocalFunction("OnEvent", "i", 4);

	printf("[N] count now %d", dynhook_count("OnEvent"));
	printf("[DONE] native2");
}
