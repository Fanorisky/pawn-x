#include <console>
#include <foreach>

// A "yield" cannot appear where stack storage is live. The inner "foreach"
// allocates loop/hidden cells on the stack, pushing the local count above the
// generator prologue's single reserved cell. Those cells are NOT lifted into
// the state block, so on resume the frame is rebuilt without them and the inner
// loop's state is garbage -- an unbounded run of "d 0" (a server hang) with no
// diagnostic. The compiler must reject it (error 099) rather than miscompile.
new g_set[8];

iterfunc Delegate()
{
	foreach (new x : g_set)
		yield return x * 100;
}

main()
{
	setinit(g_set);
	setadd(g_set, 3);
	setadd(g_set, 7);
	foreach (new v : Delegate())
		printf("d %d\n", v);
}
