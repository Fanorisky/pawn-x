#include <console>

new bool: g_pending = false;
new g_result;
new g_handle;

AddAsync(a, b) { g_result = a + b; g_pending = true; return 0; }

async Chain(x)
{
    new a = await AddAsync(x, 1);      // x+1
    new b = await AddAsync(a, 10);     // (x+1)+10
    new c = await AddAsync(b, 100);    // (x+11)+100
    printf("chain: a=%d b=%d c=%d final=%d\n", a, b, c, a + b + c + x);
}

Pump()
{
    while (g_pending)
    {
        g_pending = false;
        __async_resume(g_handle, Chain, g_result);
    }
}

main()
{
    __async_start(g_handle, Chain, 5);
    print("(chain started)");
    Pump();
}
