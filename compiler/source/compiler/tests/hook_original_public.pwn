#include <console>

// Y-Less issue #6: a user-defined `public X` runs as the chain tail, AFTER all
// hooks on X. Previously the dispatcher orphaned the user body (only "hook" ran).
forward TestPublic();
hook TestPublic() { printf("hook\n"); return 1; }
public TestPublic() { printf("public\n"); }
main() { TestPublic(); }
