#include <console>
#include <varargs>

// Runtime-index string varargs. va_strlen reads a string argument chosen at
// runtime (args after the first fixed one are 1, 2, ... as in getarg). This is
// the frame-walk the whole family shares; va_getstring/PrintArg/va_return use
// the same walk plus strcat/format/print, validated on a live host.
Test(dummy, ...)
{
	printf("len1=%d\n", va_strlen(1));
	printf("len2=%d\n", va_strlen(2));
	printf("len3=%d\n", va_strlen(3));
	#pragma unused dummy
}

main()
{
	Test(0, "hi", "seven!!", "");
}
