#if defined _INC_pawn_x
	#endinput
#endif
#define _INC_pawn_x

// pawn-x is a standalone REPLACEMENT for YSI's script-side layer, not a
// co-resident: it reserves `foreach`/`hook` as compiler keywords and provides
// its own varargs/iterator/hook stack. Detect a YSI include and stop early with
// a clear message (the per-feature includes below also guard themselves).
#if defined YSI_VERSION || defined _INC_y_va || defined _inc_y_iterate || defined _INC_y_hooks
	#error pawn-x cannot be used alongside YSI: it replaces YSI's y_va / y_iterate / y_hooks and reserves `foreach` and `hook` as native keywords. Remove the YSI includes, or the pawn-x ones — do not mix them.
#endif

// marker other code can test for (e.g. `#if defined _PAWN_X`)
#define _PAWN_X

/**
 * <library name="pawn-x">
 *  Umbrella include for the pawn-x native language features — one include for
 *  the whole stack, as a standalone replacement for the YSI script-side layer:
 *
 *    #include <pawn-x>
 *
 *  Pulls in:
 *   - <foreach>   : the `foreach` keyword + compact-set natives
 *                   (setinit/setadd/setremove/sethas/setlen/setfree/setrandom/
 *                    setget/setalloc), and ITER_STOP.
 *   - <iterators> : ready-made lazy `iterfunc` generators (Range/RangeStep/Powers).
 *   - <hook>      : the `hook` keyword's control constants
 *                   (HOOK_CONTINUE/HOOK_CONTINUE_0/HOOK_STOP/HOOK_STOP_1).
 *   - <dynhook>   : runtime hook registry + transparent interception
 *                   (dynhook_add/remove/replace/clear/count/call/intercept).
 *                   Requires the `dynhook` companion plugin at runtime; the
 *                   compiler `hook` keyword and everything else need no plugin.
 *
 *  Varargs forwarding (`___`) is a compiler keyword and needs no include.
 * </library>
 */

#include <foreach>
#include <iterators>
#include <hook>
#include <dynhook>
