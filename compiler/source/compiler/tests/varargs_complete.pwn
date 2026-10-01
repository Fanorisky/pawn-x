#include <console>
#include <varargs>

// Forward the tail: bare ___ (skips all named params) and ___3 sugar (= ___(3),
// skips the three named params dest/size/fmt) must give the same forwarding.
Fmt(dest[], size, const fmt[], {Float,_}:...)  { strformat(dest, size, false, fmt, ___); }
Fmt3(dest[], size, const fmt[], {Float,_}:...) { strformat(dest, size, false, fmt, ___3); }

// Runtime-index string-arg readers (arg 0 = tag, 1 = first vararg, 2 = second).
Inspect(tag, ...)
{
	#pragma unused tag
	printf("len1=%d\n", va_strlen(1));
	new s[32];
	va_getstring(s, 2);
	printf("get2=%s\n", s);
	new r[256];
	r = ReturnStringArg(1);
	printf("ret1=%s\n", r);
}

main()
{
	new out[64];
	Fmt(out, sizeof out, "a=%d b=%s", 7, "hi");
	printf("fmt=%s\n", out);
	Fmt3(out, sizeof out, "c=%d", 9);
	printf("fmt3=%s\n", out);
	Inspect(0, "hello", "world");
	new v[256];
	v = va_return("x=%d", 42);
	printf("vr=%s\n", v);
}
