#include <console>

// Native inline closures, Milestone 1: an inline CAPTURES the enclosing function's
// locals (read AND write) via a static link -- the enclosing frame pointer travels
// in the Callback value's {entry, FRM} record and the inline reaches the captured
// locals through it. Mutations are written straight back to the enclosing frame
// (no Callback_Restore needed). Proves the ForEach/visitor pattern y_inline exists
// for, write-back visibility, and two independent closures in one function.

ForEach(const arr[], size, Callback:cb)
{
    for (new i = 0; i < size; i++)
        cb(arr[i]);
}

CountFives(const arr[], size)
{
    new count = 0;                       // captured, mutated by the inline
    inline IsFive(v) { if (v == 5) count++; }
    ForEach(arr, size, using inline IsFive);
    return count;                        // reflects the inline's writes
}

main()
{
    new data[] = {5, 3, 5, 8, 5, 1};
    printf("fives=%d\n", CountFives(data, 6));

    new a = 0, b = 0;                    // two independent closures in one frame
    inline AddA(x) { a += x; }
    inline AddB(x) { b += x * 2; }
    ForEach(data, 6, using inline AddA);
    ForEach(data, 6, using inline AddB);
    printf("a=%d b=%d\n", a, b);
}
