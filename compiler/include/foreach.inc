#if defined _INC_foreach
	#endinput
#endif
#define _INC_foreach

/**
 * <library name="foreach">
 *  Native compact-set iteration. A script global array is used as a
 *  sorted set of distinct non-negative values:
 *
 *    new data[cap];
 *    setinit(data);
 *    setadd(data, 7);
 *    set_foreach (new i : data)
 *    {
 *        printf("value %d\n", i);   // prints each value once, ascending
 *    }
 *
 *  Layout: data[0] holds the count, data[1..count] the sorted values.
 *  `set_foreach` is a keyword (named to coexist with YSI's `foreach` macro);
 *  `set*` (setinit/setadd/setremove/sethas/setlen/setfree/setrandom/setget/
 *  setalloc) are natives.
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
 * cur == ITER_STOP (the seed). `set_foreach (new i : Gen(args))` drives it.
 */
const ITER_STOP = cellmin;
