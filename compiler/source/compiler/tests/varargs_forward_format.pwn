#include <console>
#include <string>

MyFormat(dest[], size, const fmat[], ...)
{
	new len = strformat(dest, size, false, fmat, ___(3));
	printf("formatted[%d]: %s\n", len, dest);
}

main()
{
	new str[64];
	MyFormat(str, sizeof str, "Hello %d %04x %s", 99, 0x1F, "woop");
}
