#include <console>
#include <async>

// Task 1: variable per-coroutine block size (remove the fixed 64-cell cap).
// A 100-cell array EXCEEDS the old ASYNC_BLOCK_CELLS=64 block, so under the
// uniform-stride arena Async_Alloc cleanly refused to start this coroutine.
// With right-sized blocks the array is lifted into a block that actually fits,
// filled before an "await", and summed after -- so it survives the suspend.
//   buf[i] = i  ->  sum = 0+1+...+99 = 4950.
// Looping start+complete proves the block is FREED and REUSED: every start hands
// back the same token (1) and Async_ActiveCount() returns to baseline (0).

new g_sum;

async BigArray(seed)
{
    new buf[100];
    for (new i = 0; i < 100; i++)
        buf[i] = i + seed;                 // seed=0 -> 0..99
    new got = await 0;                     // suspend; resumed by its own token
    new sum = 0;
    for (new j = 0; j < 100; j++)
        sum += buf[j];                     // must still be 4950 after the await
    g_sum = sum + got;                     // got delivered by the resume (0)
}

main()
{
    for (new n = 1; n <= 3; n++)           // start+complete repeatedly -> reuse
    {
        new t = Async_Start(BigArray, 0);
        Async_Resume(t, 0);
        printf("iter=%d token=%d active=%d\n", n, t, Async_ActiveCount());
    }
    printf("big sum=%d active=%d\n", g_sum, Async_ActiveCount());
}
