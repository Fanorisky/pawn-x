#include <console>
#include <hash>

// Runtime forward-djb2 stock. hash_rt is the lowering target for hash(expr);
// here we call it directly to pin the algorithm and known values.
main()
{
    new s[] = "gun";
    printf("h=%d\n", hash_rt(s));      // forward djb2 of "gun" = 193493135
    printf("e=%d\n", hash_rt(""));     // empty -> 5381
}
