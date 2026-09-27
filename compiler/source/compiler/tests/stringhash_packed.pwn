#include <console>
#include <hash>

// Packed strings hash to the same value as the equivalent unpacked characters,
// at compile time (packed literal fold) and runtime (ispacked-aware stock).
main()
{
    printf("fold=%d\n", hash(!"gun") == hash("gun"));   // 1: packed literal fold
    new p[] = !"gun";
    printf("rt=%d\n", hash(p) == hash("gun"));          // 1: runtime packed variable
    printf("ip=%d\n", ihash(!"GUN") == hash("gun"));    // 1: packed + case-insensitive
}
