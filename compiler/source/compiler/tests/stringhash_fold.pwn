#include <console>
#include <hash>

// Compile-time fold of hash("literal") + runtime lowering of hash(expr).
main()
{
    new const gun = hash("gun");                   // compile-time constant
    printf("gun=%d\n", gun);                       // 193493135
    printf("empty=%d\n", hash(""));                // 5381
    new s[] = "after";                             // literal AFTER a fold: address must survive
    printf("s=%s\n", s);
    new r[] = "gun";
    printf("match=%d\n", hash(r) == hash("gun"));  // runtime == compile-time -> 1
}
