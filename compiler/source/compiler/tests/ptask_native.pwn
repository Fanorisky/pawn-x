#define PLAYERS_NO_SEED
#include <ptask>

// Native path for `ptask`: with a real `native SetTimer`, the compiler must emit
// a well-formed .amx (disasm returncode 0) with the synthesised `@ptd_Name`
// dispatcher calling `__ptask_dispatch` and `@yt_init` registering it via
// SetTimer. Guards the codegen the runtime test can't reach.
native SetTimer(const func[], interval, bool:repeat);

new gc[16];
ptask PlayerTick[1000](playerid) { gc[playerid]++; }

main() { gc[0] = 0; }
