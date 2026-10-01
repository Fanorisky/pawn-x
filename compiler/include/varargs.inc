#if defined _INC_varargs
	#endinput
#endif
#define _INC_varargs

#include <core>
#include <string>
#include <console>

#if cellbits != 32
	#error varargs.inc assumes 32-bit cells (open.mp / SA-MP)
#endif

/**
 * <library name="varargs">
 *  Runtime-index string varargs. `___(N)` forwards a variadic tail from a
 *  compile-time-constant N; these read one string argument chosen at RUNTIME by
 *  walking the calling function's frame (the same trick y_va's
 *  va_strlen/va_getstring/va_return/PrintArg use), without any runtime code
 *  scan. Call them from inside a variadic function; argument numbers are 0-based
 *  as in `getarg`, so in `f(a, b, ...)` the first variadic argument is 2.
 * </library>
 */

/// <summary>Length of the string argument at runtime index `arg`.</summary>
stock va_strlen(arg)
{
	#emit load.s.pri   arg
	#emit shl.c.pri    2            // arg index -> byte offset (32-bit cells)
	#emit load.s.alt   0            // [FRM+0] = caller's frame
	#emit add
	#emit add.c        12           // skip prev-frame/ret/argcount header
	#emit load.i                    // deref: the string address the caller passed
	#emit push.pri
	#emit push.c       4
	#emit sysreq.c     strlen
	#emit stack        8
	#emit retn
	return 0;                       // never reached; defines the API
}

/// <summary>Copy the string argument at runtime index `arg` into `dest`.</summary>
stock va_getstring(dest[], arg, len = sizeof (dest))
{
	#emit load.s.pri   arg
	#emit shl.c.pri    2
	#emit load.s.alt   0
	#emit add
	#emit add.c        12
	#emit load.i
	#emit push.s       len
	#emit push.pri
	#emit load.s.alt   dest
	#emit zero.pri
	#emit stor.i                    // dest[0] = 0 so strcat acts like copy
	#emit push.alt
	#emit push.c       12
	#emit sysreq.c     strcat
	#emit stack        16
}

/// <summary>Print the string argument at runtime index `arg`.</summary>
stock PrintArg(arg)
{
	#emit load.s.pri   arg
	#emit shl.c.pri    2
	#emit load.s.alt   0
	#emit add
	#emit add.c        12
	#emit load.i
	#emit push.pri
	#emit push.c       4
	#emit sysreq.c     print
	#emit stack        8
	#emit retn
	return 0;                       // never reached; defines the API
}
