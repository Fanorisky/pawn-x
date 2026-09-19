#define AMX_OLD_CALL
#define FOREACH_NO_BOTS
#define FOREACH_NO_PLAYERS
#define FOREACH_NO_VEHICLES
#define FOREACH_NO_ACTORS
#define FOREACH_NO_LOCALS
#include <open.mp>
#include <YSI_Data\y_iterate>

#define N     400
#define ADDR  4000
#define HASR  4000000

new Iterator:gset<N>;

main()
{
	new i, r, added = 0, hits = 0, q;

	// add phase: ADDR rounds of (clear + N Iter_Add) = ADDR*N add ops
	new t0 = GetTickCount();
	for (r = 0; r < ADDR; r++)
	{
		Iter_Clear(gset);
		for (i = 0; i < N; i++)
			added += Iter_Add(gset, (i * 137) % N);
	}
	new t1 = GetTickCount();

	// has phase: HASR Iter_Contains queries, values cycling 0..N-1 (all present)
	q = 0;
	new t2 = GetTickCount();
	for (r = 0; r < HASR; r++)
	{
		hits += _:Iter_Contains(gset, q);
		if (++q >= N) q = 0;
	}
	new t3 = GetTickCount();

	printf("BENCH ysi ops RAND: add=%d ms has=%d ms  (added=%d hits=%d N=%d ADDR=%d HASR=%d)",
		t1 - t0, t3 - t2, added, hits, N, ADDR, HASR);
}
