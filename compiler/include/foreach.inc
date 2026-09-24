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

/**
 * Coroutine generators (`yield`): an `iterfunc` whose body contains
 * `yield return <expr>;` is a coroutine. Instead of being recalled once per
 * value, it runs top-to-bottom and suspends at each `yield return`, handing
 * <expr> back to the `foreach` loop; the next iteration resumes it right after
 * that point with all its locals intact. The sequence ends when the body runs
 * off the end or hits a bare `return;`.
 *
 *    iterfunc Count(n)
 *    {
 *        for (new i = 0; i != n; ++i)
 *            yield return i;         // suspends here; resumes on the next step
 *    }
 *    foreach (new v : Count(3)) { }  // v = 0, 1, 2
 *
 *  Unlike YSI, no `#define Iterator@Name iteryield` opt-in is needed — pawn-x
 *  detects the `yield` in the body and compiles the function as a coroutine.
 *  A coroutine takes ordinary user parameters (e.g. `Steps(step, target)`); it
 *  resumes from its saved continuation, so there is no `cur` seed parameter.
 *
 *  v1 restrictions (each a real diagnostic):
 *   - scalar locals only: a local array or string that spans a `yield`
 *     is rejected (error 096).
 *   - statement position only: `yield return <expr>;` is a statement, it is
 *     not an expression and cannot appear on the right of `=` (error 097).
 *   - a `yield` outside an `iterfunc` is rejected (error 095).
 *   - a coroutine cannot declare a reference (`&x`) parameter (error 098);
 *     plain scalar parameters are fine.
 *
 *  Host requirement: coroutine resume relies on `LCTRL 6` / `SCTRL 6` (get/set
 *  the code instruction pointer) — the same host dependency as YSI's yield, so
 *  any AMX host that runs YSI yield runs this.
 */
