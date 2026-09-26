#include <open.mp>
#define PP_SYNTAX
#include <PawnPlus>

// PawnPlus async task, triggered by a cross-script call from the native gamemode
// so both run in ONE server process. `await` forces this public to return and the
// plugin resumes it; the LOCAL `buf` survives via PawnPlus's frame snapshot.
forward PP_Start();
public PP_Start()
{
    print("[pawnplus] start (plugin task, frame snapshot)");
    yield 1;
    await task_ms(200);
    print("[pawnplus] tick @200ms");
    new buf[8];
    for (new i = 0; i < 8; i++) buf[i] = i * i;
    await task_ms(200);
    new sum = 0;
    for (new i = 0; i < 8; i++) sum += buf[i];
    printf("[pawnplus] done: local array survived, sum=%d", sum);
    return 1;
}
main() {}
