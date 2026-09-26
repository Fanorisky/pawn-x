#include <open.mp>

/* Host validation for the switch codegen optimization (Phase A: ranges are
 * inline bounds-checks, not per-value table records). Self-verifying: prints
 * SWITCH-HOST: PASS only if every probe classifies as expected. */

// Big range + sparse discrete: the classic "case a..b:" that used to bloat.
Classify(x)
{
    switch (x)
    {
        case -1000..-1: return 5;   // negative range
        case 0..9999:   return 1;   // the 10k-value range (was ~31KB of table)
        case 40000:     return 3;   // lone discrete far away
        default:        return 9;
    }
}

// Discrete dispatcher (typical command/dialog style, no ranges).
Dispatch(cmd)
{
    switch (cmd)
    {
        case 10: return 100;
        case 20: return 200;
        case 30: return 300;
        default: return -1;
    }
}

main()
{
    new ok = 1;

    // {input, expected} probes for Classify
    new cp[][2] = {
        {-1001, 9}, {-1000, 5}, {-1, 5}, {0, 1}, {5000, 1}, {9999, 1},
        {10000, 9}, {39999, 9}, {40000, 3}, {40001, 9}
    };
    for (new i = 0; i < sizeof cp; i++)
    {
        new got = Classify(cp[i][0]);
        if (got != cp[i][1])
        {
            printf("SWITCH-HOST: FAIL Classify(%d)=%d expected %d", cp[i][0], got, cp[i][1]);
            ok = 0;
        }
    }

    // {input, expected} probes for Dispatch
    new dp[][2] = { {9, -1}, {10, 100}, {20, 200}, {30, 300}, {31, -1} };
    for (new i = 0; i < sizeof dp; i++)
    {
        new got = Dispatch(dp[i][0]);
        if (got != dp[i][1])
        {
            printf("SWITCH-HOST: FAIL Dispatch(%d)=%d expected %d", dp[i][0], got, dp[i][1]);
            ok = 0;
        }
    }

    if (ok)
        print("SWITCH-HOST: PASS all probes classified correctly");
}
