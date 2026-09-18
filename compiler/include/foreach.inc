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
 *    Iter_Init(data);
 *    Iter_Add(data, 7);
 *    foreach (new i : data)
 *    {
 *        printf("value %d\n", i);   // prints each value once, ascending
 *    }
 *
 *  Layout: data[0] holds the count, data[1..count] the sorted values.
 *  `foreach` is a keyword; `Iter_*` are natives.
 * </library>
 */

native Iter_Init(const array[]);
native Iter_Add(const array[], value);
native Iter_Remove(const array[], value);
native Iter_Contains(const array[], value);
native Iter_Count(const array[]);
