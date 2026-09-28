#include <foreach>
#include <hook>

// Y-Less issue #7/#13: a compiler-detect symbol independent of includes, and
// break-style aliases for the stop constants.
main()
{
    #if defined __PawnX
        printf("detect=%d\n", __PawnX);
    #else
        printf("detect=0\n");
    #endif
    printf("hookbreak=%d\n", HOOK_BREAK == HOOK_STOP && HOOK_BREAK_1 == HOOK_STOP_1);
    printf("iterbreak=%d\n", ITER_BREAK == ITER_STOP);
}
