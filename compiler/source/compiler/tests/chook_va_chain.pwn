#include <console>
#include <hook>
Sum(base, ...) { new n=numargs(),s=base; for(new i=1;i<n;i++) s+=getarg(i); return s; }
hook:1  function Sum(base, ...) { return continue(base, ___) + 1000; }  // runs 2nd
hook:10 function Sum(base, ...) { return continue(base, ___) * 2; }     // runs 1st
main() { printf("r=%d\n", Sum(10, 1, 2, 3)); }
