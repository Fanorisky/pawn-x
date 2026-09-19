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
 *  `set*` (setinit/setadd/setremove/sethas/setlen) are natives.
 * </library>
 */

native setinit(const array[]);
native setadd(const array[], value);
native setremove(const array[], value);
native sethas(const array[], value);
native setlen(const array[]);
