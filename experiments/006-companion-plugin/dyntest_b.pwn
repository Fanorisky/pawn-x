// dynhook phase-B proof — transparent runtime interception of a callback.
// OnThing's own body never calls dynhook_call; the plugin inline-hooks amx_Exec
// so registered handlers fire automatically when OnThing runs, and add/remove
// take effect at runtime. Requires the `dynhook` legacy plugin.

#include <open.mp>
#include <dynhook>

forward OnThing(a);
public OnThing(a) { printf("  OnThing.original a=%d", a); return 1; }

forward ExtraA(a);
forward ExtraB(a);
public ExtraA(a) { printf("  ExtraA a=%d", a); return 1; }
public ExtraB(a) { printf("  ExtraB a=%d", a); return 1; }

main()
{
	dynhook_intercept("OnThing");

	printf("[B] fire#1 no handlers -> original only");
	CallLocalFunction("OnThing", "i", 1);

	dynhook_add("OnThing", "ExtraA");
	dynhook_add("OnThing", "ExtraB");
	printf("[B] fire#2 +ExtraA +ExtraB -> original + both (OnThing wired nothing)");
	CallLocalFunction("OnThing", "i", 2);

	dynhook_remove("OnThing", "ExtraA");
	printf("[B] fire#3 removed ExtraA -> original + ExtraB");
	CallLocalFunction("OnThing", "i", 3);

	printf("[DONE] dynB");
}
