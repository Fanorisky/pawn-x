#include <console>
#include <hash>

// Flagship use case: switch on a runtime hash, case labels are folded literals.
Classify(const s[])
{
    switch (hash(s))
    {
        case hash("gun"):    return 1;
        case hash("health"): return 2;
        case hash("car"):    return 3;
    }
    return 0;
}

main()
{
    new a[] = "gun", b[] = "car", c[] = "nope";
    printf("%d %d %d\n", Classify(a), Classify(b), Classify(c));  // 1 3 0
}
