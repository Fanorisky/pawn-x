#include <console>
#include <hook>
Sum(base, ...) { new n=numargs(),s=base; for(new i=1;i<n;i++) s+=getarg(i); return s; }
// Body is NOT variadic but the target IS -> shape mismatch (error 263).
hook function Sum(base) { return continue(base); }
main() { printf("%d\n", Sum(1,2)); }
