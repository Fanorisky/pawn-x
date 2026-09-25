#include <console>
#include <async>

// The exact example from README.md's "Native async/await" entry, made runnable.
// Single .amx, scalar locals, linear body -- the MVP envelope. A real host would
// resume the coroutine from a timer/dialog/DB completion; here main() is the pump.
AddScore(a, b) { return a + b; }

async GetScore(playerid)
{
    new base = 100;
    new s = await AddScore(playerid, base);   // suspend; resumes with the result
    printf("score=%d\n", s + base);           // 'base' survives the await
}

main()
{
    new t = Async_Start(GetScore, 7);         // starts; suspends at the await
    Async_Resume(t, AddScore(7, 100));        // deliver 107 -> resumes -> score=207
}
