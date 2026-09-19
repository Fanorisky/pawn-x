#include <open.mp>
#include <foreach>

#define N     400
#define ADDR  4000
#define HASR  4000000

new gset[N + 1];

main()
{
	new i, r, added = 0, hits = 0, q;

	// add phase: ADDR rounds of (setinit + N setadd) = ADDR*N add ops
	new t0 = GetTickCount();
	for (r = 0; r < ADDR; r++)
	{
		setinit(gset);
		for (i = 0; i < N; i++)
			added += setadd(gset, (i * 137) % N);
	}
	new t1 = GetTickCount();

	// has phase: HASR sethas queries, values cycling 0..N-1 (all present)
	q = 0;
	new t2 = GetTickCount();
	for (r = 0; r < HASR; r++)
	{
		hits += sethas(gset, q);
		if (++q >= N) q = 0;
	}
	new t3 = GetTickCount();

	printf("BENCH native ops RAND: add=%d ms has=%d ms  (added=%d hits=%d N=%d ADDR=%d HASR=%d)",
		t1 - t0, t3 - t2, added, hits, N, ADDR, HASR);
}
