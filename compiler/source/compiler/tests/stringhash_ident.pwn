#include <console>

// 'hash' must remain usable as an ordinary identifier in declaration position
// (soft keyword), while `hash(` in expression position is the intrinsic.
main()
{
    new hash = 42;
    hash = hash + 1;
    printf("hash=%d\n", hash);
}
