#include <console>
#include <hook>

/* Hook a real native (max, from <core>, auto-included via default.inc).
 * continue(...) reaches the genuine native via a direct SYSREQ. */
hook native max(value1, value2)
{
    new r = continue(value1, value2);   // real max native (SYSREQ)
    printf("max=%d\n", r);
    return r;
}

main()
{
    printf("r %d\n", max(3, 7));         // redirected to the wrapper
}
