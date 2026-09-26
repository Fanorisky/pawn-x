#include <console>
#include <async>

// MULTI-RESULT delivery (PawnPlus await_arr parity): a single "await" yields one
// cell, so to hand a coroutine several values at once the resumer calls
// Async_ResumeArr(token, values, size) -- the await returns the element COUNT and
// the values are copied out with Async_InboxArr(dest). Pure library over the resume
// seam (no compiler change). Slots are isolated, so concurrent coroutines each get
// their own result; a string is just a cell array.

async Row()
{
    new n = await 0;                    // await returns the delivered element count
    new data[8];
    new got = Async_InboxArr(data);     // copy the delivered array out
    new s = 0;
    for (new i = 0; i < got; i++) s += data[i];
    printf("row n=%d got=%d sum=%d first=%d last=%d\n", n, got, s, data[0], data[got-1]);
    return s;
}

async Named()
{
    new n = await 0;
    new name[16];
    Async_InboxArr(name);               // a string delivered as a cell array
    printf("named n=%d name=%s\n", n, name);
    return 1;
}

// A coroutine resumed by a PLAIN Async_Resume (no array) must see an EMPTY inbox,
// even when it reuses a slot a previous Async_ResumeArr coroutine left data in --
// __async_deliver resets the element count on every resume.
async NoArray()
{
    new dest[8];
    dest[0] = -9;
    new n = await 0;                    // plain resume -> await returns the value (0), no array
    new got = Async_InboxArr(dest);
    printf("noarray n=%d got=%d d0=%d\n", n, got, dest[0]);
    return 1;
}

// After a COMPOSED await (inner coroutine returns via the awaiter call.pri chain,
// which bypasses __async_deliver), the array inbox must be reset too -- otherwise a
// stale count/data from an earlier Async_ResumeArr would leak into Async_InboxArr.
async Inner() { new v = await Async_Pending(); return v; }
async Composed()
{
    new n = await 0;                    // leaf await #1: receives the array via Async_ResumeArr
    new d1[8];
    new g1 = Async_InboxArr(d1);        // {7,7,7}
    new r = await Inner();              // composed await #2 -> return-to-awaiter path
    new d2[8]; d2[0] = -5;
    new g2 = Async_InboxArr(d2);        // must be empty: composed return carried no array
    printf("composed n=%d g1=%d r=%d g2=%d d0=%d\n", n, g1, r, g2, d2[0]);
    return 1;
}

main()
{
    new t;
    new vals[5] = {10, 20, 30, 40, 50};
    t = Async_Start(Row);   Async_ResumeArr(t, vals, 5);   // await->5; sum 150
    new msg[16] = "Ada";
    t = Async_Start(Named); Async_ResumeArr(t, msg, 4);    // "Ada\0"
    t = Async_Start(NoArray); Async_Resume(t, 0);          // reuses a freed slot; inbox empty
    new three[3] = {7, 7, 7};
    t = Async_Start(Composed); Async_ResumeArr(t, three, 3);  // leaf await #1 gets {7,7,7}
    Async_ResumeInner(88);                                 // Inner completes -> composed return
    printf("active=%d\n", Async_ActiveCount());
}
