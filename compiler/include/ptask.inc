#if defined _INC_ptask
	#endinput
#endif
#define _INC_ptask

#include <players>   // the Player set (+ its connect/disconnect tracking) and foreach

/**
 * <library name="ptask">
 *  Runtime support for the native `ptask` keyword. For `ptask Name[ms](playerid)`
 *  the compiler hoists the body to a hidden public `@pt_Name(playerid)` and
 *  synthesises a no-arg dispatcher `@ptd_Name()` (registered with `SetTimer`)
 *  that calls this helper with a `Callback:` naming the body; the helper fans it
 *  out to every connected player. Public so it survives dead-code elimination
 *  (the compiler's only reference to it is synthesised after that pass).
 * </library>
 */
forward __ptask_dispatch(Callback:cb);
public __ptask_dispatch(Callback:cb)
{
	foreach (new i : Player)
		cb(i);
}
