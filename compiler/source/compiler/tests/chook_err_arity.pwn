#include <console>
#include <hook>

/* Body arg count differs from the target's signature: the target takes one
 * argument, the hook body declares two. Forwarding would skew the stack. */
Compute(a) { return a; }

hook function Compute(a, b) { return continue(a) + b; }

main() { printf("r %d\n", Compute(1)); }
