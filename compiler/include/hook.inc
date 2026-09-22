#if defined _INC_hook
	#endinput
#endif
#define _INC_hook

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
