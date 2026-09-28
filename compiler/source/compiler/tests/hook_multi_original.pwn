#include <console>
#include <hook>

// Multiple hooks run in source order, then the user's original public last.
forward Foo();
hook Foo() { printf("h1\n"); return HOOK_CONTINUE; }
hook Foo() { printf("h2\n"); return HOOK_CONTINUE; }
public Foo() { printf("orig\n"); }
main() { Foo(); }
