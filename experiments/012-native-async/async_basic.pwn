#include <console>

new bool: g_pending = false;
new g_result;
new g_handle;

AddAsync(a, b) { g_result = a + b; g_pending = true; return 0; }

async GetScore(playerid)
{
    new base = 100;
    new s = await AddAsync(playerid, base);
    printf("score=%d\n", s + base);
}

Pump()
{
    if (g_pending)
    {
        g_pending = false;
        __async_resume(g_handle, GetScore, g_result);
    }
}

main()
{
    __async_start(g_handle, GetScore, 7);
    print("(started; suspended at await, control returned to caller)");
    Pump();
}
