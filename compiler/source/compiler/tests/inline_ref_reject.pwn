#include <console>

// A by-reference parameter of the enclosing function cannot be CAPTURED by an
// inline: its frame cell holds a pointer into a caller frame, which the static
// link cannot resolve. Rejected cleanly (error 098) rather than miscompiled.
// Workaround: read it into a value local before the inline, or pass by value.

Once(Callback:cb) { cb(); }

Test(&refparam)
{
    inline Look() { printf("%d\n", refparam); }   // error 098: cannot capture a reference
    Once(using inline Look);
}

main() { new x = 99; Test(x); }
