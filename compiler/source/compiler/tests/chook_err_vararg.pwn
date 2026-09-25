#include <console>
#include <hook>

/* Hooking a variadic call-target ("printf" takes "..."). Not supported in v1:
 * the wrapper/chain forward a fixed argument count. */
hook native printf(const fmt[]) { return continue(fmt); }

main() { printf("r\n"); }
