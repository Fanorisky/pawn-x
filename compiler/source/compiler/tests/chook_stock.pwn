#include <console>
#include <hook>

/* Hooking a "stock" call target. A stock is an ordinary pawn function that
 * carries uSTOCK, so its original-endpoint is a plain "call" (identical to
 * "hook function"). continue(...) reaches the original stock body. */
stock Helper(x) { return x + 1; }

hook stock Helper(x) { return continue(x) * 2; }   // (x+1)*2

main()
{
    printf("r %d\n", Helper(3));                    // (3+1)*2 = 8
}
