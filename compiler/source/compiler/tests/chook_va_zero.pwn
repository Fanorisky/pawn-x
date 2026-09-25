#include <console>
#include <hook>
// Review focus: empty tail. printf("hi\n") has no varargs -> forwarded tail is
// zero cells. The body forwards only fmt; must not re-enter printf.
hook native printf(const fmt[], ...) { return continue(fmt, ___); }
main() { printf("hi\n"); }
