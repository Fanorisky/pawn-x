#if defined _INC_iterators
	#endinput
#endif
#define _INC_iterators

/**
 * <library name="iterators">
 *  Ready-made lazy generators for `foreach`, built on the native
 *  `iterfunc` mechanism (see <foreach>). Each computes the next value from
 *  the current one, so nothing is stored:
 *
 *    foreach (new i : Range(0, 10))     { ... }   // 0..9
 *    foreach (new i : RangeStep(0, 10, 2)) { ... }// 0 2 4 6 8
 *    foreach (new i : Powers(2, 1000))  { ... }   // 1 2 4 ... < 1000
 *
 *  The generator protocol: the first call receives cur == ITER_STOP (the
 *  seed) and returns the first value; each later call returns the next value,
 *  or ITER_STOP to end. Extra arguments are loop-invariant (evaluated once).
 *
 *  Stateful generators: a leading reference parameter (`iterfunc Name(&acc,
 *  cur, ...)`) gets a persistent hidden cell (init 0) threaded across calls, so
 *  the generator can carry more than the last emitted value (e.g. Fibonacci).
 *  (`state` is a reserved word — name the reference something else.)
 * </library>
 */

#include <foreach>   // for ITER_STOP and the foreach keyword

/**
 * Range(lo, hi): yields lo, lo+1, ..., hi-1. Empty when lo >= hi.
 */
iterfunc stock Range(cur, lo, hi)
{
	if (cur == ITER_STOP)
		return (lo < hi) ? lo : ITER_STOP;
	if (cur + 1 < hi)
		return cur + 1;
	return ITER_STOP;
}

/**
 * RangeStep(lo, hi, step): yields lo, lo+step, ... while < hi (step > 0).
 * A non-positive step yields only lo (guards against a non-terminating loop).
 */
iterfunc stock RangeStep(cur, lo, hi, step)
{
	if (cur == ITER_STOP)
		return (lo < hi) ? lo : ITER_STOP;
	if (step <= 0)
		return ITER_STOP;
	if (cur + step < hi)
		return cur + step;
	return ITER_STOP;
}

/**
 * Powers(base, limit): yields 1, base, base*base, ... while < limit.
 * Stops before an overflow would occur (base >= 2 assumed for termination;
 * base <= 1 yields only 1).
 */
iterfunc stock Powers(cur, base, limit)
{
	if (cur == ITER_STOP)
		return (1 < limit) ? 1 : ITER_STOP;
	if (base <= 1)
		return ITER_STOP;
	// guard the multiply against overflow, then against the limit
	if (cur <= (cellmax / base) && cur * base < limit)
		return cur * base;
	return ITER_STOP;
}

/**
 * N(count): yields 0, 1, ..., count-1. Empty when count <= 0. The convenience
 * short form of Range(0, count) (mirrors YSI's `N` iterator).
 */
iterfunc stock N(cur, count)
{
	if (cur == ITER_STOP)
		return (count > 0) ? 0 : ITER_STOP;
	if (cur + 1 < count)
		return cur + 1;
	return ITER_STOP;
}

/**
 * Filter(set, pred): yields the members of `set` (ascending) for which the
 * predicate returns non-zero. The predicate is an inline closure, so it can
 * capture and test the enclosing locals:
 *
 *   inline IsAdmin(id) { return IsPlayerAdmin(id); }
 *   foreach (new id : Filter(players, using inline IsAdmin)) { ... }
 *
 * pawn-x's native replacement for YSI's Filter / Null / NonNull family: those
 * are just Filter with a specific predicate.
 */
iterfunc stock Filter(cur, const src[], Callback:pred)
{
	new v = setnext(src, cur);              // cur == ITER_STOP seed -> first member
	while (v != ITER_STOP && !pred(v))
		v = setnext(src, v);
	return v;
}

/**
 * NonNull(set): yields the non-zero members of `set`, ascending. The canned
 * Filter for the common "skip the 0 slot" case, so no predicate is needed
 * (YSI's NonNull iterator).
 */
iterfunc stock NonNull(cur, const src[])
{
	new v = setnext(src, cur);
	while (v == 0)                          // skip a 0 member; ITER_STOP (cellmin) != 0 ends
		v = setnext(src, v);
	return v;
}

/**
 * Fib(limit): yields the Fibonacci numbers 1, 1, 2, 3, ... while < limit.
 * A stateful generator: the reference `acc` holds the value two positions back
 * (which `cur` alone cannot carry) and persists across iterations.
 */
iterfunc stock Fib(&acc, cur, limit)
{
	if (cur == ITER_STOP) { acc = 0; return (1 < limit) ? 1 : ITER_STOP; }
	new nxt = cur + acc;
	if (nxt >= limit)
		return ITER_STOP;
	acc = cur;
	return nxt;
}
