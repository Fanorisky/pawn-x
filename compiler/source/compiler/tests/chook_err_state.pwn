#include <console>
#include <hook>

/* State-scoped CALL hooks are not supported (v1). A "<state>" prefix on a
 * "hook native/function/stock" must be rejected with a clear diagnostic
 * instead of silently compiling as an unconditional global hook. State-scoped
 * CALLBACK hooks (e.g. "hook <running> OnTick(...)") remain valid. */
Fsm() <running> {}
Fsm() <paused>  {}

hook <running> native max(value1, value2)
{
    return continue(value1, value2);
}

main()
{
    state running;
    Fsm();
    printf("r %d\n", max(3, 7));
}
