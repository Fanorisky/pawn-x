#include <console>
#include <hook>
Sum(base, ...) { new n=numargs(),s=base; for(new i=1;i<n;i++) s+=getarg(i); return s; }
hook function Sum(base, ...) { return continue(base, ___); }
main() { printf("%d\n", Sum(1,2,3)); }
