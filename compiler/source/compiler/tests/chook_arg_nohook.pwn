#include <console>
Probe(base, ...) { return numargs(); }
main() { printf("n=%d\n", Probe(1, 2, 3)); }
