#include <console>
#include <hook>
Sum(base, ...) { new n=numargs(),s=base; for(new i=1;i<n;i++) s+=getarg(i); return s; }
hook function Sum(base, ...) { printf("body n=%d\n", numargs()); return continue(base, ___); }
main() { printf("r=%d\n", Sum(10, 1, 2, 3)); }
