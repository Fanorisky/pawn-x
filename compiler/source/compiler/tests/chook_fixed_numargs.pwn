#include <console>
#include <hook>
Add(a, b) { return a + b; }
hook function Add(a, b) { printf("n=%d\n", numargs()); return continue(a, b); }
main() { printf("r=%d\n", Add(3, 4)); }
