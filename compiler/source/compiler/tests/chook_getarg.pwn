#include <console>
#include <hook>
Sum(base, ...) { new n=numargs(),s=base; for(new i=1;i<n;i++) s+=getarg(i); return s; }
hook function Sum(base, ...) { printf("g1=%d g2=%d g3=%d\n", getarg(1), getarg(2), getarg(3)); return continue(base, ___); }
main() { printf("r=%d\n", Sum(10, 5, 6, 7)); }
