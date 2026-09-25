#include <console>
#include <hook>

/* "hook native" on a target that is a pawn function (not a native). The
 * modifier does not match the target's kind. */
ScoreFor(playerid) { return playerid * 10; }

hook native ScoreFor(playerid) { return continue(playerid); }

main() { printf("r %d\n", ScoreFor(1)); }
