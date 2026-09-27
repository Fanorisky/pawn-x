#if defined _INC_hash
	#endinput
#endif
#define _INC_hash

#include <string>   // for ispacked() (packed/unpacked-aware runtime hashing)

/**
 * <library name="hash">
 *  Native string hashing for pawn-x (the y_stringhash replacement). Four
 *  intrinsics, each a compiler keyword in call position:
 *
 *    hash(s)    djb2 ("Bernstein"), case-sensitive
 *    ihash(s)   djb2, case-INsensitive (A-Z folded to a-z)
 *    fnv1(s)    FNV-1  (32-bit)
 *    fnv1a(s)   FNV-1a (32-bit)
 *
 *  hash("literal") (a string LITERAL) folds to a compile-time constant, usable
 *  in switch/case; hash(expr) (a variable) lowers to the matching *_rt stock
 *  below. Compile-time and runtime values are identical by construction. Any
 *  byte is allowed in a literal (unlike YSI's a-z/0-9/_/space limit), and both
 *  packed and unpacked strings are supported.
 *
 *    switch (hash(params)) { case hash("gun"): ...; case hash("car"): ...; }
 * </library>
 */

#if !defined HASH_FNV_OFFSET
	#define HASH_FNV_OFFSET  (0x811C9DC5)   // 2166136261, as a cell bit pattern
	#define HASH_FNV_PRIME   (0x01000193)   // 16777619
#endif

/// <summary>djb2 hash of a string (matches hash("literal")). Packed or unpacked.</summary>
stock hash_rt(const str[])
{
	new h = 5381, c, i = 0;
	if (ispacked(str)) { while ((c = str{i++})) h = h * 33 + c; }
	else               { while ((c = str[i++])) h = h * 33 + c; }
	return h;
}

/// <summary>Case-insensitive djb2 hash (matches ihash("literal")). Packed or unpacked.</summary>
stock ihash_rt(const str[])
{
	new h = 5381, c, i = 0;
	if (ispacked(str)) { while ((c = str{i++})) { if (c >= 'A' && c <= 'Z') c += 32; h = h * 33 + c; } }
	else               { while ((c = str[i++])) { if (c >= 'A' && c <= 'Z') c += 32; h = h * 33 + c; } }
	return h;
}

/// <summary>FNV-1 (32-bit) hash of a string (matches fnv1("literal")). Packed or unpacked.</summary>
stock fnv1_rt(const str[])
{
	new h = HASH_FNV_OFFSET, c, i = 0;
	if (ispacked(str)) { while ((c = str{i++})) { h = h * HASH_FNV_PRIME; h ^= c; } }
	else               { while ((c = str[i++])) { h = h * HASH_FNV_PRIME; h ^= c; } }
	return h;
}

/// <summary>FNV-1a (32-bit) hash of a string (matches fnv1a("literal")). Packed or unpacked.</summary>
stock fnv1a_rt(const str[])
{
	new h = HASH_FNV_OFFSET, c, i = 0;
	if (ispacked(str)) { while ((c = str{i++})) { h ^= c; h = h * HASH_FNV_PRIME; } }
	else               { while ((c = str[i++])) { h ^= c; h = h * HASH_FNV_PRIME; } }
	return h;
}
