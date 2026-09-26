#include <console>
#include <async>

// A leaf "await" inside a VARIADIC call's argument list (e.g. printf) is supported,
// in ANY argument position. Reverse-order emission pushes the sibling arguments that
// follow the await before it at run time; they are pushed via push.pri, so the mid-
// expression spill (reserved from pc_exprtemp) saves and restores them across the
// suspend. Verified with lifted-variable, constant, and computed-call siblings.

side(x) { return x + 1000; }

async F()
{
    new a = 11, b = 22;
    printf("mid=%d a=%d b=%d\n", await 0, a, b);      // await arg1; lifted siblings after
    printf("last=%d\n", await 0);                     // await is the last arg
    printf("consts=%d %d %d\n", await 0, 42, 77);     // constant siblings after the await
    printf("computed=%d %d\n", await 0, side(3));     // computed-call sibling after the await
    return 1;
}

main()
{
    new t = Async_Start(F);
    Async_Resume(t, 5);   // mid=5 a=11 b=22
    Async_Resume(t, 6);   // last=6
    Async_Resume(t, 7);   // consts=7 42 77
    Async_Resume(t, 8);   // computed=8 1003
    printf("active=%d\n", Async_ActiveCount());
}
