#include <console>
#include <hash>

// ihash(): case-insensitive djb2. Folds A-Z to a-z, at compile time and runtime.
main()
{
    printf("ci=%d\n", ihash("GUN") == ihash("gun"));   // 1: case-insensitive
    printf("eq=%d\n", ihash("gun") == hash("gun"));    // 1: lowercase input == djb2
    new g[] = "GuN";
    printf("rt=%d\n", ihash(g) == ihash("gun"));       // 1: runtime, mixed case
}
