#define PLAYERS_NO_SEED
#include <console>
#include <ptask>

// Verify the ptask runtime fan-out: __ptask_dispatch loops the Player set and
// calls the callback once per connected id (this is exactly what the compiler's
// synthesised @ptd_Name dispatcher hands it). Uses `using public` (the same
// Callback value the compiler builds) so it exercises the real mechanism.
new gFired[8], gN = 0;
forward Rec(id);
public Rec(id) { gFired[gN++] = id; }

main()
{
    setadd(Player, 3);
    setadd(Player, 5);
    setadd(Player, 7);
    __ptask_dispatch(using public Rec<i>);
    printf("n=%d\n", gN);
    printf("%d %d %d\n", gFired[0], gFired[1], gFired[2]);
}
