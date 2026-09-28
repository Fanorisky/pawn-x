#include <console>
#include <foreach>

// Y-Less issue #1: foreach is removal-safe (no skip / no duplicate) when
// setremove runs mid-loop -- current element, a future element, and Reverse().
new d[20];
main()
{
    setinit(d); for (new k=1;k<=5;k++) setadd(d,k);
    foreach (new i : d) { if (i==2) setremove(d,i); printf("a %d\n",i); }   // 1 2 3 4 5

    setinit(d); for (new k=1;k<=5;k++) setadd(d,k);
    foreach (new i : d) { if (i==2) setremove(d,4); printf("b %d\n",i); }   // 1 2 3 5

    setinit(d); for (new k=1;k<=5;k++) setadd(d,k);
    foreach (new i : Reverse(d)) { if (i==4) setremove(d,i); printf("c %d\n",i); } // 5 4 3 2 1
}
