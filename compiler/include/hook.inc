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
 *
 *  State-scoped hooks: prefix a Pawn automaton state so a hook fires only in
 *  that state (the dispatcher checks the automaton's state variable and skips
 *  the hook otherwise):
 *
 *    hook <running> OnPlayerText(playerid, text[]) { ... }   // only while "running"
 *    hook <ClientState:auth> OnPlayerText(playerid, text[]) { ... }  // named automaton
 *
 *  The automaton must be genuinely used elsewhere (a state-conditioned function
 *  or state variable), as in normal Pawn. `hook:N` priority and `<state>` may be
 *  combined: `hook:100 <running> OnFoo(a)`.
 *
 *  ----------------------------------------------------------------------------
 *  Call-site hooks: hook native / hook function / hook stock
 *  ----------------------------------------------------------------------------
 *  The callback `hook` above intercepts a public/callback. A *call-site* hook
 *  intercepts an ordinary in-script call to a named native, pawn function, or
 *  stock — every call to that target within the compilation unit is routed
 *  through the hook chain instead of straight to the target:
 *
 *    hook native random(range)     { ... }   // every random(...) call
 *    hook function ComputeScore(p) { ... }   // every ComputeScore(...) call
 *    hook stock Helper(a, b)       { ... }   // every Helper(...) call
 *
 *  Use the modifier that matches the target's kind: `hook native` for a real
 *  native, `hook function` / `hook stock` for a pawn function or stock. This is
 *  pure compile-time call redirection — the call site is emitted as a call to a
 *  synthesised wrapper; there is no runtime bytecode scan or code-segment
 *  rewrite. It corrects the old "pawn-x cannot hook a real native/function"
 *  gap versus YSI 5.10 (see docs/audits/2026-09-24-hooks-vs-ysi510.md), and
 *  does it natively rather than by rewriting the code segment.
 *
 *  continue(...) — invoke the rest of the chain / the original:
 *
 *    hook function ComputeScore(playerid)
 *    {
 *        new r = continue(playerid);   // run the next hook, else the original
 *        return r + 1;
 *    }
 *
 *  Inside a call-hook body, `continue(args)` calls the next hook in the chain;
 *  past the last hook it calls the ORIGINAL (the real native via SYSREQ, or the
 *  original pawn/stock body). Its return value is whatever the remainder of the
 *  chain returns. You may:
 *   - call it once for normal pass-through (optionally reading/adjusting the
 *     result, as above);
 *   - modify the arguments before forwarding — `continue(x * 2)` forwards the
 *     changed value to the original;
 *   - call it zero times to REPLACE the target entirely (return your own value,
 *     the original never runs);
 *   - call it more than once to invoke the original several times.
 *  `continue(...)` is only valid inside a call-hook body; the bare `continue;`
 *  loop statement is unaffected.
 *
 *  Chains + priority work exactly like callback hooks: multiple hooks on one
 *  target run in `hook:N` priority order (higher first, equal = source order),
 *  each `continue`-ing down to the next until the original at the tail.
 *
 *  Forward references are honoured: a call written textually BEFORE the `hook`
 *  declaration is still redirected through the chain.
 *
 *  Variadic (`...`) targets:
 *
 *    Sum(base, ...) { new n = numargs(), s = base;
 *                     for (new i = 1; i < n; i++) s += getarg(i); return s; }
 *
 *    hook function Sum(base, ...)
 *    {
 *        new r = continue(base, ___);   // forward the fixed arg + the whole tail
 *        return r * 2;
 *    }
 *
 *  A variadic target is call-site-hookable: declare the hook body with the same
 *  `fixed…, ...` shape as the target (a shape mismatch — variadic body on a
 *  fixed target or vice versa — is error 263). Inside a variadic hook body:
 *   - `numargs()` / `getarg(n)` / `setarg(n)` see the USER argument list; `n`
 *     is the user index (the hidden per-hook chain index is invisible, so
 *     `numargs()` and the indices match what the original target sees).
 *   - `getarg`/`setarg` are meaningful only on the variadic TAIL. Fixed
 *     by-value params are read by their name (`base` above), as in standard
 *     Pawn — not through `getarg`.
 *   - forward the tail with `continue(fixed…, ___)`: pass each fixed arg
 *     (its current, possibly-modified value) then `___` to splice the entire
 *     variable-argument tail onto the next hook / the original.
 *   - or forward EVERYTHING with a bare `continue()`: it re-forwards all of the
 *     current arguments (fixed params by their current value + the full tail),
 *     equivalent to `continue(fixed…, ___)` without spelling the fixed args out.
 *
 *  v1 limits (all enforced at compile time — the misuse is a hard error):
 *   - The hook body's argument list must match the target's signature exactly,
 *     including its variadic shape (`fixed…, ...` vs `fixed…`); an arity or
 *     shape mismatch is rejected.
 *   - Using the wrong modifier for the target's kind is rejected, as is an
 *     unknown target (no such native or function).
 *   - Single compilation unit (in-`.amx`), like the callback `hook` — cross-.amx
 *     call-site hooking is out of scope.
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
