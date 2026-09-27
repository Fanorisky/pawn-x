#include <console>
#include <timers>

// Stubs standing in for the host timer natives (pawnruns has no SetTimer);
// they let us verify the <timers> wrappers forward correctly.
stock SetTimer(const func[], interval, bool:repeat)
{
    printf("ST %s %d %d\n", func, interval, repeat);
    return 42;
}
stock SetTimerEx(const func[], interval, bool:repeat, const format[], {Float,_}:...)
{
    printf("STE %s %d %d %s\n", func, interval, repeat, format);
    return 43;
}
stock KillTimer(timerid)
{
    printf("KT %d\n", timerid);
    return 1;
}

main()
{
    new Timer:t = Timer_Repeat("OnTick", 1000);
    printf("t=%d\n", _:t);
    Timer_Once("OnceOff", 5000);
    Timer_RepeatEx("OnHit", 250, "dd", 7, 9);
    Timer_Stop(t);
}
