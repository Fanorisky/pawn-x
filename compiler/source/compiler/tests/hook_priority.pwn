#include <console>
#include <hook>

// "hook:N" sets chain priority: higher N runs earlier; equal priority (or the
// default 0) keeps source order. Lets independent includes order their hooks
// without depending on include order.
hook:10 OnFoo(a)  { printf("P10 %d\n", a);  return HOOK_CONTINUE; }
hook OnFoo(a)     { printf("P0a %d\n", a);  return HOOK_CONTINUE; }   // default 0
hook:100 OnFoo(a) { printf("P100 %d\n", a); return HOOK_CONTINUE; }
hook OnFoo(a)     { printf("P0b %d\n", a);  return HOOK_CONTINUE; }   // default 0
hook:-5 OnFoo(a)  { printf("Pneg %d\n", a); return HOOK_CONTINUE; }

main()
{
	OnFoo(7);   // runs in priority order: P100, P10, P0a, P0b, Pneg
}
