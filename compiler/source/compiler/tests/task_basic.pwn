#include <console>

// "task" lowers to the pawn-x runtime name Timer_Set (bound to the host's
// SetTimer by <timers>). pawnruns has no host timer; this stub binds Timer_Set
// and records the calls. The warmup call keeps the stub live and shows the
// convention; the second line proves the synthesised `task` auto-registration.
stock Timer_Set(const func[], interval, bool:repeat)
{
    printf("ST %s %d %d\n", func, interval, repeat);
    return 7;
}

forward OnGameModeInit();

new gCount = 0;
task Ticker[1000]() { ++gCount; }

main()
{
    Timer_Set("warmup", 1, false);  // keeps the stub live; shows the convention
    OnGameModeInit();               // fires the synthesised @yt_init registration
    printf("count=%d\n", gCount);
}
