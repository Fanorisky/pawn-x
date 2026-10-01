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

// Longest string ReturnStringArg / va_return will materialise. Kept
// parenthesis-free so it can be a `#emit push.c` operand.
#if !defined VA_MAX_STRING
	#define VA_MAX_STRING 256
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

/// <summary>Return the string argument at runtime index `arg` (so a function
/// can return one of its string varargs). Same shape as YSI's ReturnStringArg.
/// Reads the caller's frame directly (delegating to va_getstring would add a
/// frame level and read the wrong args).</summary>
stock ReturnStringArg(arg)
{
	static buf[VA_MAX_STRING];
	new size = VA_MAX_STRING;       // #emit reads a symbol, not a macro
	#emit load.s.pri   arg
	#emit shl.c.pri    2            // arg index -> byte offset (32-bit cells)
	#emit load.s.alt   0            // [FRM+0] = caller's frame
	#emit add
	#emit add.c        12           // skip prev-frame/ret/argcount header
	#emit load.i                    // deref: the string address the caller passed
	#emit push.s       size
	#emit push.pri
	#emit const.alt    buf
	#emit zero.pri
	#emit stor.i                    // buf[0] = 0 so strcat acts like copy
	#emit push.alt
	#emit push.c       12
	#emit sysreq.c     strcat
	#emit stack        16
	return buf;
}

/// <summary>Format the varargs into a string and return it (YSI va_return).
/// Cleaner than YSI's stack-pop trick: it just forwards the tail to `format`
/// with the native `___`. The buffer is static, so copy the result out before
/// the next call.</summary>
stock va_return(const fmat[], {Float,_}:...)
{
	static buf[VA_MAX_STRING];
	strformat(buf, sizeof buf, false, fmat, ___);
	return buf;
}

// y_va / y_utils spelling aliases.
#define getstringarg    va_getstring
#define GetStringArg    va_getstring
#define getstring       ReturnStringArg
#define GetString       ReturnStringArg
#define returnstringarg ReturnStringArg
#define printarg        PrintArg

// Forward-tail sugar (y_va): ___0 .. ___9 are ___(0) .. ___(9), and
// va_start<N> is ___(N). ___(N) forwards the argument list skipping the first
// N parameters, so from f(a, b, ...) you write ___(2) to forward just the tail
// (bare ___ already skips all the named params).
#define ___0 ___(0)
#define ___1 ___(1)
#define ___2 ___(2)
#define ___3 ___(3)
#define ___4 ___(4)
#define ___5 ___(5)
#define ___6 ___(6)
#define ___7 ___(7)
#define ___8 ___(8)
#define ___9 ___(9)
#define va_start<%0> ___(%0)
