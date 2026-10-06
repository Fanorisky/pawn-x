#define PLAYERS_NO_SEED
// open.mp provides SetTimer; declare it before <timers> (whose stocks call it).
native SetTimer(const func[], interval, bool:repeat);
native SetTimerEx(const func[], interval, bool:repeat, const fmt[], {Float,_}:...);
native KillTimer(timerid);
#include <timers>

// Regression: a script that uses `task` but never otherwise references the
// Timer_Set stock must STILL emit a real SetTimer registration. Before the fix,
// dead-code elimination dropped the Timer_Set stock (its only caller, the
// synthesised @yt_init, is created after DCE), so @yt_init's call resolved to a
// wrong address and SetTimer was never issued -> the task silently never fired.
// Assert the compiled code actually contains a `sysreq ... SetTimer`, i.e. the
// task's auto-registration reaches the host timer native.

new gc;
task Beat[500]() { gc++; }

main() { gc = 0; }
