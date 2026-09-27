#include <console>

// "inline const": captured enclosing locals are READ-ONLY. Assigning one is a
// compile error (022); reading is fine (see inline_capture for the mutable form).

Once(Callback:cb) { cb(); }

Test()
{
    new count = 5;
    inline const Look() { count = count + 1; }   // error 022: count is read-only
    Once(using inline Look);
    printf("%d\n", count);
}

main() { Test(); }
