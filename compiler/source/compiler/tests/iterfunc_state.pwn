#include <console>
#include <foreach>

// Stateful generator: a leading reference parameter (&acc) is a persistent
// hidden cell threaded across iterations, so a generator can remember more than
// the last emitted value. Fibonacci needs the value two-back, which `cur` alone
// cannot carry. (NB: `state` is a reserved word; name the reference otherwise.)
iterfunc stock Fib(&acc, cur, limit)
{
	if (cur == ITER_STOP) { acc = 0; return 1; }   // first: emit 1, prev = 0
	new nxt = cur + acc;
	if (nxt >= limit) return ITER_STOP;
	acc = cur;                                      // remember current as next "prev"
	return nxt;
}

main()
{
	foreach (new i : Fib(50)) printf("fib %d\n", i);   // 1 1 2 3 5 8 13 21 34
	printf("done\n");
}
