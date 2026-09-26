#include <console>
#include <string>
#include <async>

/* Full-context async: a STRING BUILT into a local buffer BEFORE an "await" and
 * printed intact AFTER the coroutine resumes. The buffer is a lifted local, so
 * its characters live in the coroutine's arena state block across the suspend
 * (no save/restore). strcopy/strcat write into the lifted buffer before the
 * await; the resume reads it back byte-for-byte unchanged.
 *   name = "player-" + "42" = "player-42" (len 9); survives the await. */
AddScore(a, b) { return a + b; }

async Greeting(id)
{
    new name[32];
    strcopy(name, "player-");             // build the string BEFORE the await
    strcat(name, "42");
    new before = strlen(name);            // 9

    new got = await AddScore(id, 0);      // suspend; pump resumes with 7

    printf("got=%d\n", got);
    printf("name=%s len=%d\n", name, strlen(name));   // intact: "player-42", 9
    printf("stable=%d\n", strlen(name) == before);    // 1 (string survived)
}

main()
{
    new t = Async_Start(Greeting, 1);
    printf("%s\n", "(started; suspended at await)");
    Async_Resume(t, 7);
}
