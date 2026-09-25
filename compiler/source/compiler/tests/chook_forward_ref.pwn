#include <console>
#include <hook>

/* A call to a hooked pawn function that appears LEXICALLY BEFORE the "hook"
 * declaration. YSI hooks ALL calls regardless of source order, so this pre-decl
 * call MUST redirect through the wrapper and observe the hooked value (15), not
 * the raw original (10). This proves forward-reference redirection. */
Target(x) { return x; }

Caller() { return Target(10); }        // call site is BEFORE the hook below

hook function Target(x) { return continue(x) + 5; }   // continue(10)+5 = 15

main()
{
    printf("r %d\n", Caller());         // must be 15, proving the pre-decl call redirected
}
