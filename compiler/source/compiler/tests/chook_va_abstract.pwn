#include <console>
#include <hook>
Sum(base, ...) { new n=numargs(),s=base; for(new i=1;i<n;i++) s+=getarg(i); return s; }
hook function Sum(base, ...) { return continue() + 1; }   // abstract: forward base + tail
main() { printf("r=%d\n", Sum(10, 1, 2, 3)); }
