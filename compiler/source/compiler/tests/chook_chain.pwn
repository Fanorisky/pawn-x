#include <console>
#include <hook>

Base(x) { printf("base %d\n", x); return x; }

// Declared lower-priority-first ON PURPOSE: the priority sort (not source order)
// must place hook:10 (A) ahead of hook:1 (B). Without the sort the chain would
// run B before A and the output order would flip -> this test guards the sort.
hook:1  function Base(x) { printf("B %d\n", x); return continue(x) + 10; }  // runs second
hook:10 function Base(x) { printf("A %d\n", x); return continue(x) + 1; }   // runs first

main() { printf("r %d\n", Base(7)); }
