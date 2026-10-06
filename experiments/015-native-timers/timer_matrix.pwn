// ============================================================================
// Timer matrix: exercise every pawn-x <timers> path on a real open.mp host and
// log tagged events for an automated checker. Covers the combinations that a
// Mode-only run can never reach (task-only registration, defer arg forwarding,
// one-shot vs repeating, ptask fan-out over REAL connected NPCs incl. mid-run
// join/quit, stop/Timer_Stop, and a self-rescheduling timer).
//
// Each line is "[M] <tag> ..." so the harness can grep/count without ambiguity.
// A global frame counter (GlobalTick) drives the scripted NPC join/quit/stop so
// the test is deterministic in wall-clock-free "ticks".
// ============================================================================
#include <open.mp>
#include <foreach>
#include <players>
#include <ptask>
#include <timers>
#include <hook>

main() {}

new gFrame = 0;           // advanced by Drive[500]
new Timer:gStoppable = Timer:0;
new gStoppableHits = 0;
new gNpcA = INVALID_NPC_ID, gNpcB = INVALID_NPC_ID;

// ---- 1) task: repeating no-arg, auto-registered (the bug's ground zero) -----
new gBeat = 0;
task Beat[500]()
{
    printf("[M] beat %d", ++gBeat);
}

// ---- 2) ptask: per-connected-player fan-out, auto-registered ----------------
ptask PerPlayer[500](playerid)
{
    printf("[M] ptask pid=%d isnpc=%d", playerid, IsPlayerNPC(playerid));
}

// ---- 3) timer + defer: one-shot, no args (must fire EXACTLY once) -----------
timer OnceNoArg[400]()
{
    printf("[M] once_noarg");
}

// ---- 4) timer + defer: ARG FORWARDING int/float/string ----------------------
timer WithArgs[400](i, Float:f, const s[])
{
    printf("[M] withargs i=%d f=%.2f s=%s", i, f, s);
}

// ---- 5) timer + repeat: repeating WITH an int arg ---------------------------
timer RepArg[500](x)
{
    printf("[M] reparg x=%d", x);
}

// ---- 6) self-rescheduling one-shot (re-entrancy: schedules itself again) ----
timer ReSchedule[300](n)
{
    printf("[M] resched n=%d", n);
    if (n < 3)
        defer ReSchedule[300](n + 1);
}

// ---- 7) stoppable repeating timer, killed via Timer_Stop at frame 4 ---------
forward Stoppable();
public Stoppable()
{
    printf("[M] stoppable %d", ++gStoppableHits);
}

// ---- driver: advances frames, scripts the scenario deterministically --------
task Drive[500]()
{
    gFrame++;
    printf("[M] frame %d players=%d", gFrame, setlen(Player));

    if (gFrame == 1) {
        // kick off the one-shots and arg-forwarding cases
        defer OnceNoArg[400]();
        defer WithArgs[400](42, 3.14, "hello");
        repeat RepArg[500](7);
        defer ReSchedule[300](1);
        gStoppable = Timer_Repeat("Stoppable", 500);
        printf("[M] stoppable_started handle=%d", _:gStoppable);
    }
    if (gFrame == 2) {
        // bring a real player online mid-run -> ptask must start hitting it
        gNpcA = NPC_Create("MatrixBot_A");
        printf("[M] npc_a_create id=%d", gNpcA);
    }
    if (gFrame == 4) {
        gNpcB = NPC_Create("MatrixBot_B");
        printf("[M] npc_b_create id=%d", gNpcB);
        Timer_Stop(gStoppable);
        printf("[M] stoppable_stopped_at_frame=%d hits=%d", gFrame, gStoppableHits);
    }
    if (gFrame == 7) {
        // drop one player mid-run -> ptask fan-out must shrink
        if (gNpcA != INVALID_NPC_ID) {
            NPC_Destroy(gNpcA);
            printf("[M] npc_a_destroyed");
        }
    }
}

hook OnPlayerConnect(playerid)
{
    printf("[M] connect pid=%d isnpc=%d len=%d", playerid, IsPlayerNPC(playerid), setlen(Player));
    return 1;
}

hook OnPlayerDisconnect(playerid, reason)
{
    printf("[M] disconnect pid=%d len=%d", playerid, setlen(Player));
    return 1;
}
