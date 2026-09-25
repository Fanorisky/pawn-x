#include <console>
#include <async>

/* The "async" keyword prefixes a FUNCTION declaration (it makes that function a
 * scheduler-driven coroutine). Applying it to something that is not a function
 * has no meaning and must be rejected with a dedicated error. */
async NotAFunction;

main() { printf("x\n"); }
