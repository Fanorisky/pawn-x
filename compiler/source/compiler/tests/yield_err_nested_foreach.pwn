#include <console>
#include <foreach>

// A "yield" cannot appear where stack storage is live. Iterating a PLAIN SET
// inside a generator is fine (its walk state is lifted into the state block --
// see yield_foreach_sugar), but iterating ANOTHER GENERATOR is not: the
// coroutine operand parks its state-block base and per-step argument cells on
// the stack, and those are not lifted. On resume the frame is rebuilt without
// them and the inner driver's state is garbage -- an unbounded hang with no
// diagnostic. The compiler must reject it (error 099) rather than miscompile.

iterfunc Leaf()
{
	yield return 1;
	yield return 2;
}

iterfunc Delegate()
{
	foreach (new v : Leaf())
		yield return v * 100;
}

main()
{
	foreach (new x : Delegate())
		printf("d %d\n", x);
}
