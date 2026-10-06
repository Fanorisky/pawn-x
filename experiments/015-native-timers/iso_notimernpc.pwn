// Isolation B: manual SetTimer vs auto-registered task, side by side.
// If manual fires but task doesn't -> auto-registration chain is the bug.
// If neither fires -> SetTimer/dispatch itself is dead in a minimal gamemode.
#include <open.mp>
#include <foreach>
#include <players>
#include <ptask>
#include <timers>
#include <hook>

main() {}

new gT = 0, gM = 0;

forward ManualBeat();
public ManualBeat() { printf("[iso] MANUAL beat #%d", ++gM); }

hook OnGameModeInit()
{
    setadd(Player, 0);
    new h = SetTimer("ManualBeat", 500, true);
    printf("[iso] init Player.len=%d manualTimer=%d", setlen(Player), h);
    return 1;
}

task Beat[500]()
{
    printf("[iso] AUTO beat #%d", ++gT);
    return 1;
}
