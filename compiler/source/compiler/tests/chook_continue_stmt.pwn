#include <console>
#include <hook>

/* "continue(...)" (the call-hook chain-advance intrinsic) written as a bare
 * STATEMENT inside a hook body -- for side effects, not as a value. The docs
 * invite calling the original multiple times; here the body invokes the
 * original twice and then returns its own value. A plain loop "continue;"
 * (see the for-loop below) must remain unaffected. */
Target(x) { printf("orig %d\n", x); return x; }

hook function Target(x)
{
    continue(x);            // bare statement: run the original (result discarded)
    for (new i = 0; i < 2; i++) {
        if (i == 0)
            continue;       // plain loop-continue, unaffected
        printf("loop %d\n", i);
    }
    continue(x + 1);        // bare statement again: run the original once more
    return 999;
}

main()
{
    printf("r %d\n", Target(5));
}
