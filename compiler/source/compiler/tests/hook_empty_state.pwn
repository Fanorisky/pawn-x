#include <console>
// Y-Less issue #13 side-note: an empty state list "<>" must be a clean error,
// never a compiler crash.
hook <> X() { printf("x\n"); }
main() {}
