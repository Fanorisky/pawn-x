#include <console>
#include <async>

/* Task 2 (full-context async): a MULTI-DIMENSIONAL array local of an "async"
 * coroutine, written before an "await" and read after it. The whole flattened
 * array -- its indirection vector AND its data cells -- is LIFTED into the
 * coroutine's arena state block. The indirection entries are byte offsets
 * relative to the array base (position-independent), so building the vector at
 * the lifted base B+addr and having address() return B+addr is enough: indexing
 * grid[i][j] follows automatically and the cells survive the suspend with no
 * save/restore.
 *   grid[i][j] = seed + i*3 + j  ->  seed=10:
 *     row0 = 10 11 12   row1 = 13 14 15   sum = 75. */
AddScore(a, b) { return a + b; }

async GridAcross(seed)
{
    new grid[2][3];
    for (new i = 0; i < 2; i++)
        for (new j = 0; j < 3; j++)
            grid[i][j] = seed + i * 3 + j;   // written BEFORE the await

    new got = await AddScore(seed, 0);       // suspend; pump resumes with 99

    printf("got=%d\n", got);
    new sum = 0;
    for (new r = 0; r < 2; r++)
        for (new c = 0; c < 3; c++)
            sum += grid[r][c];               // must still be 75 after the await
    printf("row0: %d %d %d\n", grid[0][0], grid[0][1], grid[0][2]);
    printf("row1: %d %d %d\n", grid[1][0], grid[1][1], grid[1][2]);
    printf("sum=%d\n", sum);
}

main()
{
    new t = Async_Start(GridAcross, 10);
    printf("%s\n", "(started; suspended at await)");
    Async_Resume(t, 99);
}
