#include <console>
#include <hook>
Sum2(a, b, ...) { new n=numargs(),s=a+b; for(new i=2;i<n;i++) s+=getarg(i); return s; }
hook function Sum2(a, b, ...) { return continue(a, b, ___); }
main() { printf("r=%d\n", Sum2(10, 20, 1, 2, 3)); }
