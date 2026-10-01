#define PLAYERS_NO_SEED
#include <ptask>

// Native path for `ptask`: the registration lowers to the pawn-x runtime name
// Timer_Set (bound to the host SetTimer by <timers>). With a real
// `native Timer_Set` the compiler must emit a well-formed .amx (disasm
// returncode 0) with the synthesised `@ptd_Name` dispatcher calling
// `__ptask_dispatch` and `@yt_init` registering it via Timer_Set. Guards the
// codegen the runtime test can't reach.
native Timer_Set(const func[], interval, bool:repeat);

new gc[16];
ptask PlayerTick[1000](playerid) { gc[playerid]++; }

main() { gc[0] = 0; }
