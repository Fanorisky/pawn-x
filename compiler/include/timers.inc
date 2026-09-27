#if defined _INC_timers
	#endinput
#endif
#define _INC_timers

/**
 * <library name="timers">
 *  Thin, plugin-free wrappers over the host timer natives
 *  (`SetTimer`/`SetTimerEx`/`KillTimer`, provided by the SA-MP / open.mp
 *  includes). Tagged `Timer:` handles and readable names, no bytecode scan and
 *  no auto-registration magic: you name the callback and the interval, the
 *  wrapper schedules it. For deferred work that must keep local state alive
 *  across the wait, prefer <async> (`await Async_Ms(ms)`), which is the pawn-x
 *  path for that; these wrappers are for plain named-callback timers.
 *
 *    new Timer:t = Timer_Repeat("OnTick", 1000);   // every second
 *    Timer_Once("OnceOff", 5000);                    // once, after 5s
 *    Timer_RepeatEx("OnHit", 250, "dd", id, dmg);    // repeating, with args
 *    Timer_Stop(t);
 *
 *  Requires the host timer natives to be declared (they are, in any SA-MP /
 *  open.mp script). The argument-forwarding variants use pawn-x `___` varargs.
 * </library>
 */

/// <summary>Schedule <c>func</c> to fire every <c>interval</c> ms. Returns the timer handle.</summary>
stock Timer:Timer_Repeat(const func[], interval)
	return Timer:SetTimer(func, interval, true);

/// <summary>Schedule <c>func</c> to fire once after <c>interval</c> ms. Returns the timer handle.</summary>
stock Timer:Timer_Once(const func[], interval)
	return Timer:SetTimer(func, interval, false);

/// <summary>Repeating timer with arguments forwarded to the callback (see SetTimerEx format).</summary>
stock Timer:Timer_RepeatEx(const func[], interval, const format[], {Float,_}:...)
	return Timer:SetTimerEx(func, interval, true, format, ___);

/// <summary>One-shot timer with arguments forwarded to the callback (see SetTimerEx format).</summary>
stock Timer:Timer_OnceEx(const func[], interval, const format[], {Float,_}:...)
	return Timer:SetTimerEx(func, interval, false, format, ___);

/// <summary>Stop a timer started by one of the above.</summary>
stock Timer_Stop(Timer:id)
	return KillTimer(_:id);
