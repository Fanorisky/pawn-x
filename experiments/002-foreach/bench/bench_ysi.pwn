#define AMX_OLD_CALL
#define FOREACH_NO_BOTS
#define FOREACH_NO_PLAYERS
#define FOREACH_NO_VEHICLES
#define FOREACH_NO_ACTORS
#define FOREACH_NO_LOCALS
#include <open.mp>
#include <YSI_Data\y_iterate>

#define N     400
#define REPS  200000

new Iterator:gset<N>;

main()
{
	new i, r, acc = 0;
	for (i = 0; i < N; i++)
		Iter_Add(gset, i);
	new t0 = GetTickCount();
	for (r = 0; r < REPS; r++)
		foreach (new v : gset)
			acc += v;
	new t1 = GetTickCount();
	printf("BENCH ysi foreach: %d ms  acc=%d  (N=%d REPS=%d)", t1 - t0, acc, N, REPS);
}
