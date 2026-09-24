#include <console>
#include <hook>

Real(x) { printf("SHOULD NOT RUN %d\n", x); return x; }

hook function Real(x) { return x * 3; }   // no continue()

main() { printf("r %d\n", Real(4)); }
