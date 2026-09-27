#include <console>
#include <hash>

// Characters outside YSI's a-z/0-9/_/space set hash correctly (better-than-YSI).
main()
{
    new s[] = "A/B.C!";
    printf("ok=%d\n", hash(s) == hash("A/B.C!"));   // 1
}
