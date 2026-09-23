#include <console>
#include <hook>

// A live automaton with two states. It must be genuinely used (here Fsm() is
// called) so its state variable is allocated — a state referenced only by a
// hook, with no other live use, would not be (normal Pawn state behaviour).
Fsm() <running> {}
Fsm() <paused>  {}

// "hook <state> Name(...)" fires only while the automaton is in <state>; the
// dispatcher checks the automaton's state variable at runtime and skips it
// otherwise. The plain hook always runs.
hook <running> OnTick(a) { printf("only-running %d\n", a); return HOOK_CONTINUE; }
hook OnTick(a)           { printf("always %d\n", a);       return HOOK_CONTINUE; }

main()
{
	state running;
	Fsm();
	OnTick(1);          // both fire
	state paused;
	Fsm();
	OnTick(2);          // only the plain hook fires
	printf("done\n");
}
