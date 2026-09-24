#include <console>
#include <hook>

Two(x) { printf("orig %d\n", x); return x; }

hook function Two(x)
{
    new a = continue(x);
    new b = continue(x + 100);
    return a + b;
}

main() { printf("r %d\n", Two(1)); }
