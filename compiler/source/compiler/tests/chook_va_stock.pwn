#include <console>
#include <hook>
stock Sum(base, ...) { new n=numargs(),s=base; for(new i=1;i<n;i++) s+=getarg(i); return s; }
hook stock Sum(base, ...) { return continue(base, ___) * 2; }
main() { printf("r=%d\n", Sum(10, 1, 2, 3)); }
