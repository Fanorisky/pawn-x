#include <console>
#include <hook>

/* A REAL native call lexically BEFORE the "hook native" declaration. Task 4
 * makes forward references redirect too: this pre-hook call is now ROUTED
 * THROUGH THE HOOK, exactly like a post-hook call (YSI hooks all calls
 * regardless of source order). The two-pass crux is that this pre-hook native
 * call must emit "call wrapper" (2 cells) in BOTH the final addressing pass and
 * the write pass -- never "sysreq" (4 cells) in one and "call" in the other --
 * or every following address is corrupted. It must also COMPILE and RUN
 * correctly (no double native-id / no runtime error 19). */
early()
{
    return max(10, 20);         // forward ref: now redirected through the hook
}

hook native max(value1, value2)
{
    new r = continue(value1, value2);   // real max native (SYSREQ)
    printf("hooked=%d\n", r);
    return r;
}

main()
{
    printf("early %d\n", early());      // pre-hook  call -> hooked (continue(10,20)=20)
    printf("late %d\n", max(3, 7));     // post-hook call -> hooked (continue(3,7)=7)
}
