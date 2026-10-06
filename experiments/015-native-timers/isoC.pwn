// Isolation E: force-reference the Timer_Set STOCK (not the SetTimer native).
// If AUTO beat fires now, the bug is: the task/ptask Timer_Set/Timer_SetEx
// binding stock is dead-code-eliminated before @yt_init (synthesized later)
// references it, so auto-registration calls a wrong address.
#include <open.mp>
#include <foreach>
#include <players>
#include <timers>
#include <hook>

new gKeep;
main() { gKeep = Timer_Set("Noop", 3600000, false); }   // keep the stock alive

forward Noop();
public Noop() {}

new gT = 0;

task Beat[500]()
{
    printf("[isoC] AUTO beat #%d", ++gT);
    return 1;
}
