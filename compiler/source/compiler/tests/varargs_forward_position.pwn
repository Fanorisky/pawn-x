#include <console>
#include <string>

new dest[64];

Fixed(a)
{
	printf("fixed: %d\n", a);
}

BadNamed(const fmat[], ...)
{
	strformat(dest, sizeof dest, false, ___);
	#pragma unused fmat
}

BadTail(...)
{
	Fixed(1, ___);
}

main()
{
	BadNamed("%d", 1);
	BadTail(2);
}
