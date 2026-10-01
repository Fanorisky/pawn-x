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
 *  REPLACEMENT for YSI's `foreach`, not a co-resident of it -- do not include
 *  YSI's y_iterate/y_foreach in the same script.
 * </library>
 */

// The set* operations. By DEFAULT they are pure-Pawn stocks, so `foreach` and
// the set need NO plugin and run on any AMX host. Define FOREACH_NATIVE_SET
// before including to use the faster C natives instead (those need the set
// runtime, e.g. iterset.so on open.mp). Layout: data[0] = count, data[1..count]
// = sorted, distinct, non-negative values.
#if defined FOREACH_NATIVE_SET
native setinit(array[]);
native setadd(array[], value);
native setremove(array[], value);
native sethas(const array[], value);
native setlen(const array[]);
native setfree(const array[]);
native setrandom(const array[]);
native setget(const array[], index);
native setalloc(array[]);
native setnext(const array[], value);
native setprev(const array[], value);
#else
// lower_bound: smallest slot in [1..count] whose value is >= v, else count+1.
static stock XSet_LB(const a[], v)
{
	new lo = 1, hi = a[0];
	while (lo <= hi)
	{
		new mid = (lo + hi) >> 1;
		if (a[mid] == v)
			return mid;
		if (a[mid] < v)
			lo = mid + 1;
		else
			hi = mid - 1;
	}
	return lo;
}
stock setinit(array[])
	array[0] = 0;
stock setlen(const array[])
	return array[0];
stock sethas(const array[], value)
{
	new pos = XSet_LB(array, value);
	return (pos <= array[0] && array[pos] == value) ? 1 : 0;
}
stock setget(const array[], index)
	return (0 <= index < array[0]) ? array[1 + index] : -1;
stock setadd(array[], value, size = sizeof array)
{
	if (value < 0)
		return 0;                       // non-negative values only
	new count = array[0];
	new pos = XSet_LB(array, value);
	if (pos <= count && array[pos] == value)
		return 0;                       // already present
	if (count + 1 >= size)
		return 0;                       // full: refuse rather than run off the end
	for (new i = count; i >= pos; --i)  // shift the tail up one slot
		array[i + 1] = array[i];
	array[pos] = value;
	array[0] = count + 1;
	return 1;
}
stock setremove(array[], value)
{
	new count = array[0];
	new pos = XSet_LB(array, value);
	if (pos > count || array[pos] != value)
		return 0;                       // absent
	for (new i = pos; i < count; ++i)   // shift the tail down one slot
		array[i] = array[i + 1];
	array[0] = count - 1;
	return 1;
}
stock setfree(const array[])
{
	new count = array[0], expected = 0;
	for (new i = 1; i <= count; ++i)
	{
		if (array[i] == expected)
			++expected;
		else if (array[i] > expected)
			break;                      // first gap
	}
	return expected;
}
stock setalloc(array[], size = sizeof array)
{
	new id = setfree(array);
	setadd(array, id, size);
	return id;
}
stock setrandom(const array[])
{
	new count = array[0];
	return count != 0 ? array[1 + random(count)] : -1;
}
stock setnext(const array[], value)
{
	new count = array[0];
	new pos = XSet_LB(array, value);
	if (pos <= count && array[pos] == value)
		++pos;                          // skip the equal value: want strictly greater
	return (pos <= count) ? array[pos] : cellmin;
}
stock setprev(const array[], value)
{
	new pos = XSet_LB(array, value) - 1;   // slot before the first >= value
	return (pos >= 1) ? array[pos] : cellmin;
}
#endif


// Capacity-aware queries. The set's capacity is its array size minus the count
// slot; `size` is filled by the compiler from `sizeof` at the call site, so you
// just call `setcap(myset)`. These are the native replacements for YSI's
// Iter_Size / Iter_Available / Iter_IsFull / Iter_IsEmpty.

/// <summary>How many values the set can hold (array size minus the count slot).</summary>
stock setcap(const array[], size = sizeof array)
	return size - 1;

/// <summary>Free slots remaining before the set is full.</summary>
stock setspace(const array[], size = sizeof array)
	return (size - 1) - setlen(array);

/// <summary>1 when the set has no free slots left, else 0.</summary>
stock setisfull(const array[], size = sizeof array)
	return setlen(array) >= size - 1;

/// <summary>1 when the set holds no values, else 0.</summary>
stock setisempty(const array[])
	return setlen(array) == 0;

/// <summary>Smallest member (first in ascending order), or -1 if empty.
/// The native replacement for YSI's Iter_First / cursor Begin.</summary>
stock setfirst(const array[])
	return setget(array, 0);

/// <summary>Largest member (last in ascending order), or -1 if empty.
/// The native replacement for YSI's Iter_Last.</summary>
stock setlast(const array[])
{
	new n = setlen(array);
	return n != 0 ? setget(array, n - 1) : -1;
}

/**
 * Lazy generators (`iterfunc`): a function called once per iteration that
 * returns the next value, or ITER_STOP to end. The first call receives
 * cur == ITER_STOP (the seed). `foreach (new i : Gen(args))` drives it.
 */
const ITER_STOP = cellmin;
const ITER_BREAK = cellmin;   // alias of ITER_STOP: `break`-style naming (mirrors pawn's `break`)

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
 *  Unlike YSI, no `#define Iterator@Name iteryield` opt-in is needed -- pawn-x
 *  detects the `yield` in the body and compiles the function as a coroutine.
 *  A coroutine takes ordinary user parameters (e.g. `Steps(step, target)`); it
 *  resumes from its saved continuation, so there is no `cur` seed parameter.
 *
 *  Locals across a `yield`: scalar locals AND fixed-size arrays/strings
 *  (including multi-dimensional) may span a `yield` -- they are lifted into the
 *  generator state block, so no stack snapshot is needed.
 *
 *  `foreach` over a set across a `yield`: a `foreach (new v : someSet)` may
 *  appear directly inside a coroutine and its body may `yield`. The loop
 *  variable and the walk state (set base + current-member cursor) are lifted
 *  into the state block, so they survive the suspend; the advance is
 *  value-based (`setnext`/`setprev`), so it stays removal-safe and works for
 *  `Reverse()`. This does NOT extend to iterating ANOTHER generator from inside
 *  a coroutine (`foreach (v : Gen())` then `yield`): that parks the driver's
 *  state on the stack and is still rejected (error 099).
 *
 *  v1 restrictions (each a real diagnostic):
 *   - statement position only: `yield return <expr>;` is a statement, it is
 *     not an expression and cannot appear on the right of `=` (error 097).
 *   - a `yield` outside an `iterfunc` is rejected (error 095).
 *   - a coroutine cannot declare a reference (`&x`) parameter (error 098);
 *     plain scalar parameters are fine.
 *   - a `yield` while iterating another generator (`foreach (v : Gen())`) is
 *     rejected (error 099); a `yield` inside a plain `for`/`while`, or inside a
 *     `foreach` over a set, is fine.
 *
 *  Host requirement: coroutine resume relies on `LCTRL 6` / `SCTRL 6` (get/set
 *  the code instruction pointer) -- the same host dependency as YSI's yield, so
 *  any AMX host that runs YSI yield runs this.
 */
