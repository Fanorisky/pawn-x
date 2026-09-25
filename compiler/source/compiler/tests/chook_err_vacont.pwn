#include <console>
#include <hook>
Add(a, b) { return a + b; }
hook function Add(a, b) { return continue(a, ___); }   // Add is not variadic -> ___ invalid
main() { printf("%d\n", Add(1, 2)); }
