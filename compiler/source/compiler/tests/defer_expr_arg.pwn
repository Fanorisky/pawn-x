#define PLAYERS_NO_SEED
native SetTimer(const func[], interval, bool:repeat);
native SetTimerEx(const func[], interval, bool:repeat, const fmt[], {Float,_}:...);
native KillTimer(timerid);
#include <timers>

// Regression: a "defer"/"repeat" argument that is an EXPRESSION with a constant
// right operand (e.g. n + 1) must be evaluated correctly. Before the fix,
// emit_timer_schedule parsed the argument without stage-buffering, so plnge2()'s
// stgdel() (which scratches the pushed left operand when the right operand is a
// constant) was a no-op: the pushed "n" was orphaned and ALT was loaded with the
// constant, so "n + 1" compiled to "1 + 1". The correct codegen loads n, then
// adds the constant via add.c -- assert the add.c form is present.
new g;
timer Step[300](n) { g = n; defer Step[300](n + 1); }

main() { g = 0; defer Step[300](1); }
