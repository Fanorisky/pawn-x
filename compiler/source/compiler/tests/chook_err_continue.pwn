#include <console>
#include <hook>

/* "continue(...)" (the call-hook chain-advance intrinsic) used in a normal
 * function, outside any hook native/function/stock body. The loop statement
 * "continue;" is unaffected -- only the parenthesised call form is rejected. */
Normal()
{
    continue(1);
}

main() { printf("r %d\n", 0); Normal(); }
