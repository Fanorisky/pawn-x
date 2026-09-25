#include <console>
#include <hook>

/* "hook function" on an undefined target: no such native or function exists. */
hook function Nope(x) { return continue(x); }

main() { printf("r %d\n", 0); }
