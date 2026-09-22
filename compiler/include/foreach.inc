#if defined _INC_foreach
	#endinput
#endif
#define _INC_foreach

// pawn-x reserves `foreach` as a compiler KEYWORD, so it cannot coexist with
// YSI's `foreach` macro. If YSI's iterator lib was included first, stop now with
// a clear message instead of a cryptic keyword clash deeper in the build.
#if defined _inc_y_iterate || defined FOREACH_VERSION || defined _Y_ITERATE_LOCAL_VERSION
	#error pawn-x <foreach> conflicts with YSI y_iterate/y_foreach: pawn-x reserves `foreach` as a native keyword. Include one or the other, not both (pawn-x is a standalone replacement for YSI's iterators).
#endif

/**
 * <library name="foreach">
 *  Native compact-set iteration. A script global array is used as a
 *  sorted set of distinct non-negative values:
 *
 *    new data[cap];
 *    setinit(data);
 *    setadd(data, 7);
 *    foreach (new i : data)
 *    {
 *        printf("value %d\n", i);   // prints each value once, ascending
 *    }
 *
 *  Layout: data[0] holds the count, data[1..count] the sorted values.
 *  `foreach` is a compiler keyword; `set*` (setinit/setadd/setremove/sethas/
 *  setlen/setfree/setrandom/setget/setalloc) are natives.
 *
 *  Note: pawn-x reserves `foreach` as a keyword, so it is a standalone
 *  REPLACEMENT for YSI's `foreach`, not a co-resident of it — do not include
 *  YSI's y_iterate/y_foreach in the same script.
 * </library>
 */

native setinit(const array[]);
native setadd(const array[], value);
native setremove(const array[], value);
native sethas(const array[], value);
native setlen(const array[]);
native setfree(const array[]);     // smallest non-negative integer not in the set
native setrandom(const array[]);   // a random member, or -1 if empty
native setget(const array[], index);   // value at 0-based ascending position, or -1
native setalloc(const array[]);    // allocate + return the smallest free id

/**
 * Lazy generators (`iterfunc`): a function called once per iteration that
 * returns the next value, or ITER_STOP to end. The first call receives
 * cur == ITER_STOP (the seed). `foreach (new i : Gen(args))` drives it.
 */
const ITER_STOP = cellmin;
