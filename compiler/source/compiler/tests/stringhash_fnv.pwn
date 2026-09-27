#include <console>
#include <hash>

// fnv1() / fnv1a(): 32-bit FNV variants, compile-time fold + runtime, must agree.
main()
{
    printf("empty=%d\n", fnv1("") == HASH_FNV_OFFSET && fnv1a("") == HASH_FNV_OFFSET); // 1
    printf("algo=%d\n", fnv1("hello") != fnv1a("hello"));   // 1: FNV-1 differs from FNV-1a
    printf("nd=%d\n", fnv1("gun") != hash("gun"));          // 1: FNV differs from djb2
    new g[] = "gun";
    printf("rt=%d\n", fnv1(g) == fnv1("gun") && fnv1a(g) == fnv1a("gun")); // 1
}
