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
