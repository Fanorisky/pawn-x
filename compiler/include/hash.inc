#if defined _INC_hash
	#endinput
#endif
#define _INC_hash

/**
 * <library name="hash">
 *  Native string hashing for pawn-x. The `hash()` intrinsic (a compiler
 *  keyword in call position) folds a string LITERAL to a compile-time
 *  constant, and lowers `hash(expr)` (a non-literal argument) to a call to
 *  the `hash_rt` stock below. Compile-time and runtime values are identical
 *  by construction.
 *
 *    switch (hash(params))          // runtime hash of a variable
 *    {
 *        case hash("gun"):    ...   // folded to a constant at compile time
 *        case hash("health"): ...
 *    }
 *
 *  Algorithm: forward djb2 ("Bernstein"), h = h*33 + c per character over an
 *  unpacked string, 32-bit wrap (natural in cell arithmetic). Case-sensitive.
 *  Any byte is allowed in a literal (unlike YSI's a-z/0-9/_/space limit).
 * </library>
 */

/// <summary>Runtime forward-djb2 hash of a string; matches hash("literal") exactly.</summary>
/// <param name="str">The (unpacked, NUL-terminated) string to hash.</param>
/// <returns>The djb2 hash as a cell.</returns>
stock hash_rt(const str[])
{
	new h = 5381;
	for (new i = 0; str[i]; ++i)
		h = h * 33 + str[i];	// 32-bit wrap is automatic in cell math
	return h;
}
