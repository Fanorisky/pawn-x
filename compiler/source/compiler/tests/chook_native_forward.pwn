#include <console>
#include <hook>

/* A REAL native call lexically BEFORE the "hook native" declaration. In Task 3
 * this call is NOT redirected -- it runs the genuine (un-hooked) native. Task 4
 * will make forward references redirect too. The point of this test is that the
 * forward reference COMPILES and RUNS correctly (no double native-id / no
 * runtime error 19). */
early()
{
    return max(10, 20);         // un-hooked: raw native max(10,20) == 20
}

hook native max(value1, value2)
{
    new r = continue(value1, value2);   // real max native (SYSREQ)
    printf("hooked=%d\n", r);
    return r;
}

main()
{
    printf("early %d\n", early());      // pre-hook  call -> raw native (20)
    printf("late %d\n", max(3, 7));     // post-hook call -> redirected to the hook (7)
}
