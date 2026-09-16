#include <console>

// CallLocalFunction is an open.mp/SA-MP host native; it is not part of the
// vendored compiler/include nor of the pawnruns runner (console/string/core
// natives only). Declaring it here lets this test capture that the exact
// dynamic-dispatch passthrough pattern compiles; running it needs the
// open.mp server environment (see experiments/001-varargs/RESULT.md).
native CallLocalFunction(const function[], const format[], ...);

forward Target(a, b);

public Target(a, b)
{
	printf("target: %d %d\n", a, b);
}

Dispatch(...)
{
	CallLocalFunction("Target", "ii", ___);
}

main()
{
	Dispatch(11, 22);
}
