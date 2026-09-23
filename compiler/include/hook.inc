#if defined _INC_hook
	#endinput
#endif
#define _INC_hook

// pawn-x reserves `hook` as a compiler KEYWORD, so it cannot coexist with YSI's
// `hook` macro. If YSI y_hooks was included first, fail early and clearly.
#if defined _INC_y_hooks || defined Y_HOOKS_CONTINUE_RETURN_1
	#error pawn-x <hook> conflicts with YSI y_hooks: pawn-x reserves `hook` as a native keyword. Include one or the other, not both (pawn-x is a standalone replacement for YSI's hooks).
#endif

/**
 * <library name="hook">
 *  Native single-unit multi-hook support (pawn-x experiment 004).
 *
 *  A callback normally has exactly one implementation; a second body is
 *  "error 021: symbol already defined". The <c>hook</c> keyword lifts that for
 *  one compilation unit:
 *
 *    hook OnPlayerConnect(playerid)
 *    {
 *        // handler A
 *        return HOOK_CONTINUE;
 *    }
 *
 *    hook OnPlayerConnect(playerid)
 *    {
 *        // handler B
 *        return HOOK_CONTINUE;
 *    }
 *
 *  Every <c>hook Name(...)</c> body in the unit runs, in source order, when
 *  Name is invoked. Each body returns a control value that decides whether the
 *  chain continues and what the callback ultimately returns. The compiler
 *  synthesises a single "public Name(...)" dispatcher that chains the bodies.
 *
 *  This is pure compiler codegen: the dispatcher is an ordinary public that
 *  calls the hidden per-hook functions with plain push/call/branch. No runtime
 *  bytecode scanning, no new opcodes. Cross-.amx hook chaining is out of scope
 *  (the host already dispatches callbacks to every script).
 *
 *  Ordering: hooks run in source order by default. Prefix a priority to override
 *  it without relying on include order:
 *
 *    hook:100 OnPlayerConnect(playerid) { ... }   // runs earlier
 *    hook:-10 OnPlayerConnect(playerid) { ... }    // runs later
 *
 *  Higher priority runs first; equal priority (default 0) keeps source order.
 *
 *  Default return: by default the callback returns the last hook's chain value.
 *  To force a fall-through default (like YSI's HOOK_RET — e.g. a command
 *  callback that must default to 0 unless a hook claims it), declare:
 *
 *    hook default OnPlayerCommandText = 0;
 *
 *  Then a hook returns HOOK_STOP_1 to claim it (return 1); if none do, the
 *  chain falls through to the declared default instead of the CONTINUE value.
 * </library>
 */

/**
 * <summary>Run the next hook in the chain; the chain result so far is 1.</summary>
 */
const HOOK_CONTINUE   = 1;

/**
 * <summary>Run the next hook in the chain; the chain result so far is 0.</summary>
 */
const HOOK_CONTINUE_0 = 0;

/**
 * <summary>Stop the chain; the callback returns 0.</summary>
 */
const HOOK_STOP       = -1;

/**
 * <summary>Stop the chain; the callback returns 1.</summary>
 */
const HOOK_STOP_1     = -2;
