#include <console>
#include <hook>
// Hook the real variadic native printf; forward fmt + the tail unchanged.
// The body must NOT call printf (would re-enter the wrapper) -> forward only.
hook native printf(const fmt[], ...) { return continue(fmt, ___); }
main() { printf("x=%d y=%d\n", 7, 9); }
