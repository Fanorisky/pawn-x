#include <console>
#include <hook>
Add(a, b) { return a + b; }
hook function Add(a, b) { return continue() * 10; }   // abstract on a fixed-arity hook
main() { printf("r=%d\n", Add(3, 4)); }
