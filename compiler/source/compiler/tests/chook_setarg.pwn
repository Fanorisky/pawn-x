#include <console>
#include <hook>
Sum(base, ...) { new n=numargs(),s=base; for(new i=1;i<n;i++) s+=getarg(i); return s; }
hook function Sum(base, ...) { setarg(1, 0, 100); return continue(base, ___); }
main() { printf("r=%d\n", Sum(10, 5, 6, 7)); }
