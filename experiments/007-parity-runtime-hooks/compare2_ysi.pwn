// YSI — the same runtime-handler scenario, done the only way YSI allows.
// YSI hook chains are fixed at COMPILE time (ALS): every handler must be
// compiled in as a `hook` body, and the chain can never grow, shrink, or gain a
// handler that wasn't known at compile time. The nearest equivalent to
// "enable/disable a handler at runtime" is to compile them ALL in and gate each
// with a boolean flag. You cannot truly add or remove, nor register a public
// decided at runtime.

#define AMX_OLD_CALL
#define FOREACH_NO_BOTS
#define FOREACH_NO_PLAYERS
#define FOREACH_NO_VEHICLES
#define FOREACH_NO_ACTORS
#define FOREACH_NO_LOCALS
#define PAWN_X_NO_NATIVE_FTOUCH
#define PAWN_X_NO_YSI_ITERATORS
#include <open.mp>
#include <YSI_Coding\y_hooks>

new bool:gAdminLog = false;
new bool:gMetrics  = false;

forward OnEvent(a);
hook OnEvent(a) { printf("  base OnEvent a=%d", a); return 1; }
#include <YSI_Internal\y_unique>
hook OnEvent(a) { if (gAdminLog) printf("  [flag] AdminLog a=%d", a); return 1; }
#include <YSI_Internal\y_unique>
hook OnEvent(a) { if (gMetrics)  printf("  [flag] Metrics a=%d", a);  return 1; }
#include <YSI_Internal\y_unique>

main()
{
	print("[Y] t0 base only");
	CallLocalFunction("OnEvent", "i", 1);

	gAdminLog = true;                       // NOT a real add — a compiled-in flag
	print("[Y] t1 +AdminLog (flag toggle; handler was hard-compiled in)");
	CallLocalFunction("OnEvent", "i", 2);

	gMetrics = true;
	print("[Y] t2 +Metrics (flag toggle)");
	CallLocalFunction("OnEvent", "i", 3);

	gAdminLog = false;                      // NOT a real remove — still in the chain
	print("[Y] t3 -AdminLog (flag toggle; body still runs, just gated out)");
	CallLocalFunction("OnEvent", "i", 4);

	print("[Y] chain size is fixed at 3 forever; cannot add a runtime-decided handler");
	printf("[DONE] ysi2");
}
