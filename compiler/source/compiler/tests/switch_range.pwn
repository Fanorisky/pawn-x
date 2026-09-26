#include <console>

/* Sparse ranges + a lone discrete value + a negative range.
 * Guards range dispatch, boundaries, gaps, negatives, and default. */
Classify(x)
{
    switch (x)
    {
        case -10..-1:    return 5;
        case 0..9:       return 1;
        case 1000..1009: return 2;
        case 5000:       return 3;
        default:         return 9;
    }
}

main()
{
    new probe[] = { -11, -10, -1, 0, 5, 9, 10, 999, 1000, 1009, 1010, 4999, 5000, 5001 };
    for (new i = 0; i < sizeof probe; i++)
        printf("%d\n", Classify(probe[i]));
}
