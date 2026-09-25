#include <console>
#include <async>

/* A by-REFERENCE parameter of an "async" coroutine (here "&x", but an array
 * parameter behaves identically -- see async_err_array_across) is a pointer
 * into the CALLER's frame storage. That storage is freed when the coroutine
 * suspends at an "await", so reading the parameter after resume would be a
 * use-after-free. The compiler must reject it with a dedicated diagnostic
 * rather than lift it (impossible) or silently miscompile. Copy-in semantics
 * are deferred future work. */
async Bad(&x)
{
    new v = await x;
    printf("%d\n", v);
}

main() { new n = 7; Async_Start(Bad, n); }
