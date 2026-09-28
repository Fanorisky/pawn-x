#include <console>

// pawnruns has no SA-MP SetTimer; this stub records the calls. The explicit
// warmup call keeps the stub emitted and shows the plain call convention; the
// second line proves the compiler's synthesised `task` auto-registration.
stock SetTimer(const func[], interval, bool:repeat)
{
    printf("ST %s %d %d\n", func, interval, repeat);
    return 7;
}

forward OnGameModeInit();

new gCount = 0;
task Ticker[1000]() { ++gCount; }

main()
{
    SetTimer("warmup", 1, false);   // keeps the stub live; shows the convention
    OnGameModeInit();               // fires the synthesised @yt_init registration
    printf("count=%d\n", gCount);
}
