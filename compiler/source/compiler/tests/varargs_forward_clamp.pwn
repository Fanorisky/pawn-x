#include <console>
#include <core>

Callee(...)
{
	printf("clamp: numargs=%d\n", numargs());
}

Wrapper(...)
{
	Callee(___(5));
}

main()
{
	Wrapper(1, 2, 3);
}
