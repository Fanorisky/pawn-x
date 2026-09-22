// dynhook phase-A proof — compiled with the pawn-x pawncc.
// Demonstrates the runtime capability the compile-time `hook` cannot: add,
// remove, and replace handlers while the server is running. Requires the
// `dynhook` legacy plugin (config.json legacy_plugins ["dynhook"]).

#include <open.mp>
#include <dynhook>

forward H1(a);
forward H2(a);
forward H3(a);
public H1(a) { printf("  H1 a=%d", a); return 1; }
public H2(a) { printf("  H2 a=%d", a); return 1; }
public H3(a) { printf("  H3 a=%d", a); return 1; }

forward OnMsg(id, const s[]);
public OnMsg(id, const s[]) { printf("  OnMsg id=%d s=%s", id, s); return 1; }

main()
{
	dynhook_add("Ev", "H1");
	new c = dynhook_add("Ev", "H2");
	printf("[DYN] added H1,H2 count=%d", c);

	printf("[DYN] call#1 expect H1,H2 invoked=%d", dynhook_call("Ev", "i", 7));

	dynhook_remove("Ev", "H1");
	printf("[DYN] removed H1 count=%d", dynhook_count("Ev"));
	printf("[DYN] call#2 expect H2 invoked=%d", dynhook_call("Ev", "i", 8));

	dynhook_replace("Ev", "H2", "H3");
	printf("[DYN] replaced H2->H3");
	printf("[DYN] call#3 expect H3 invoked=%d", dynhook_call("Ev", "i", 9));

	printf("[DYN] cleared dropped=%d", dynhook_clear("Ev"));
	printf("[DYN] call#4 expect none invoked=%d", dynhook_call("Ev", "i", 10));

	// string + int forwarding
	dynhook_add("Msg", "OnMsg");
	printf("[DYN] call#5 string forward invoked=%d", dynhook_call("Msg", "is", 42, "hello"));

	printf("[DONE] dyn");
}
