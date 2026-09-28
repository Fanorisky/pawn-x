#if defined _INC_ysi_compat
	#endinput
#endif
#define _INC_ysi_compat

#include <foreach>

/**
 * <library name="ysi_compat">
 *  Optional YSI-to-pawn-x migration shims (Y-Less issue #1, note #2). Include
 *  this to keep YSI `y_iterate` spelling working on the native pawn-x set, so
 *  porting is closer to a no-op than a find-and-replace:
 *
 *    new Iterator:players<MAX_PLAYERS>;   // -> players[MAX_PLAYERS + 1]
 *    Iter_Add(players, playerid);         // -> setadd(...)
 *    foreach (new i : players) { ... }    // unchanged
 *
 *  The `+ 1` is the crux: a pawn-x set stores its count in slot 0, so a YSI
 *  iterator of N values needs N+1 cells. Declaring it via `Iterator:name<N>`
 *  gets the size right automatically (the same trick y_iterate used to hide the
 *  extra cell), avoiding the silent under-sizing a manual `new name[N]` causes.
 *
 *  These are plain aliases; new code should prefer the native `set*` names.
 * </library>
 */

// Sized-iterator declaration: reserve the value slots + the count slot.
#define Iterator:%0<%1> %0[(%1) + 1]

// Function-name aliases (y_iterate -> pawn-x set natives).
#define Iter_Add(%0)       setadd(%0)
#define Iter_Remove(%0)    setremove(%0)
#define Iter_Contains(%0)  sethas(%0)
#define Iter_Count(%0)     setlen(%0)
#define Iter_Clear(%0)     setinit(%0)
#define Iter_Alloc(%0)     setalloc(%0)
#define Iter_Free(%0)      setfree(%0)
#define Iter_Random(%0)    setrandom(%0)
