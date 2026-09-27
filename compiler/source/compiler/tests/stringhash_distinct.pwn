#include <console>
#include <hash>

// Distinct strings hash distinctly; fold == runtime including a 32-bit-wrapping
// (long) string.
main()
{
    new x[] = "listen", y[] = "silent";
    new z[] = "this is a deliberately long string to overflow 32 bits abcdefgh";
    printf("d=%d\n", hash("listen") != hash("silent"));   // 1
    printf("wrap=%d\n", hash(z) == hash("this is a deliberately long string to overflow 32 bits abcdefgh")); // 1
    printf("xy=%d\n", hash(x) == hash("listen") && hash(y) == hash("silent")); // 1
}
