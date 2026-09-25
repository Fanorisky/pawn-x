#include <console>
// compile-fail: "await" is only valid inside an "async" function
Bar() { new x = await 3; printf("%d", x); }
main() { Bar(); }
