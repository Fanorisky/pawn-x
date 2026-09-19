#include <open.mp>

#define N     400
#define REPS  200000

new gset[N + 1];

main()
{
	new i, r, acc = 0;
	gset[0] = N;                       // compact layout: [count, v1..vN]
	for (i = 0; i < N; i++)
		gset[i + 1] = i;               // hand-filled sorted set 0..N-1
	new t0 = GetTickCount();
	for (r = 0; r < REPS; r++)
		set_foreach (new v : gset)
			acc += v;
	new t1 = GetTickCount();
	printf("BENCH native foreach: %d ms  acc=%d  (N=%d REPS=%d)", t1 - t0, acc, N, REPS);
}
