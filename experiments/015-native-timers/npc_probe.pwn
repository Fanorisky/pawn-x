// Probe: does open.mp NPC_Create() fill a real player slot so <players>'s
// Player set grows (OnPlayerConnect fires) and ptask fans out to it?
// This is the riskiest assumption of the timer-matrix test; verify it alone first.
#include <open.mp>
#include <foreach>
#include <players>
#include <ptask>
#include <timers>
#include <hook>

main() {}   // without an entry point open.mp reports "bad entry point" and re-inits

new gTicks = 0;
new bool:gInit = false;

hook OnGameModeInit()
{
    if (gInit) return 1;              // guard: only act once
    gInit = true;
    print("[probe] OnGameModeInit");
    printf("[probe] Player len at init = %d", setlen(Player));
    new a = NPC_Create("TimerBot_A");
    new b = NPC_Create("TimerBot_B");
    printf("[probe] NPC_Create A=%d B=%d", a, b);
    return 1;
}

hook OnPlayerConnect(playerid)
{
    printf("[probe] OnPlayerConnect playerid=%d isNPC=%d Player.len=%d",
        playerid, IsPlayerNPC(playerid), setlen(Player));
    return 1;
}

// no-arg repeating task: proves task fires, and prints the Player set each beat
task Beat[500]()
{
    printf("[probe] beat #%d Player.len=%d", ++gTicks, setlen(Player));
    return 1;
}

// per-player task: the real question - does it fan out to the connected NPCs?
ptask PerPlayer[500](playerid)
{
    printf("[probe] ptask playerid=%d", playerid);
    return 1;
}
