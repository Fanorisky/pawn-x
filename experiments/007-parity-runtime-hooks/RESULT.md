# Experiment 007: Parity Comparison #2 — RUNTIME hook manipulation

**Date:** 2026-09-22
**Server:** open.mp `omp-server` (real runtime, both scripts run live)
**Follows:** exp 005 (which marked "runtime hook add/remove/replace" as native's
one gap) and exp 006 (the companion plugin that closed it).
**Question:** now that pawn-x has the `dynhook` companion, how does native
compare to YSI on the *runtime* hook story?

Two gamemodes, same scenario: a game event `OnEvent(a)` whose **handler set
changes while the server is running** — enable admin logging, add metrics, then
disable admin logging, all at runtime.

- `compare2_native.pwn` — pawn-x pawncc + `dynhook` plugin. Uses
  `dynhook_intercept` + `dynhook_add`/`dynhook_remove` (Phase B transparent
  interception).
- `compare2_ysi.pwn` — stock pawncc + YSI 5. Uses `hook OnEvent(a)` bodies.

## Observable result: same handler SET fires per tick

Both fire the **same handlers each tick** (the runtime chain is a PRE-hook, so
native runs the added handlers just before `base`; YSI runs `base` first because
it is the first compiled `hook` body — order differs, membership is identical):

| tick | handlers that fire |
|---|---|
| t0 | base |
| t1 | base + AdminLog |
| t2 | base + AdminLog + Metrics |
| t3 | base + Metrics |

So for the toggle case the two are behaviourally equal. **The difference is in
what each had to do to get there — and what each *cannot* do.**

## How each achieves it (the real finding)

**Native (true runtime mutation).** `AdminLog` and `Metrics` are ordinary
publics, *not* wired as hooks anywhere. They are attached and detached at
runtime:
```pawn
dynhook_add("OnEvent", "AdminLog");     // t1: handler joins the chain, live
dynhook_add("OnEvent", "Metrics");      // t2
dynhook_remove("OnEvent", "AdminLog");  // t3: handler actually leaves
```
`dynhook_count("OnEvent")` goes 0 → 1 → 2 → 1 — the chain genuinely grows and
shrinks.

**YSI (fixed chain + flags).** YSI hook chains are built at COMPILE time (ALS).
Every handler must be compiled in as a `hook OnEvent(a)` body, and the chain can
never change size. The only way to "toggle a handler at runtime" is to compile
them all in and gate each with a boolean:
```pawn
hook OnEvent(a) { if (gAdminLog) printf(...); return 1; }  // always in the chain
...
gAdminLog = true;   // not an add — just un-gating a hard-compiled body
```

## What native can do that YSI structurally cannot

1. **Register a handler decided at runtime.** `dynhook_add("OnEvent", name)`
   takes a public *name* — it can attach a handler chosen at runtime, e.g. one
   living in a module/filterscript loaded after the gamemode compiled. YSI's
   chain is frozen at compile time; a handler that wasn't compiled as a `hook`
   body can never join.
2. **Truly remove a handler** (not just gate it out) — the body stops being
   called and the chain shrinks. YSI's gated body still runs every dispatch.
3. **Change chain size at runtime.** Native: 0↔N. YSI: fixed at the compiled
   count forever.

## What YSI still has (honest)

YSI's compile-time chain has **zero per-dispatch overhead** (direct ALS calls,
no name lookup). Native's runtime chain costs an `amx_FindPublic` + `amx_Exec`
per handler per dispatch. For handlers that never change, the compiler `hook`
(exp 004) is the right tool and matches YSI's cost; `dynhook` is for the cases
that must change at runtime. Two-pillar: use each where it fits.

## Correction to exp 005

Exp 005 listed "runtime add/remove/replace (`DEFINE_HOOK_REPLACEMENT`)" as
YSI's edge. On inspection `DEFINE_HOOK_REPLACEMENT` is a compile/init-time
**name-shortening** table (maps a long callback prefix to a short one), *not* a
runtime handler API. YSI has **no** runtime handler add/remove. So this is a
capability pawn-x's companion **adds**, that YSI never had — not a gap native was
missing.

## Files
`compare2_native.pwn`, `compare2_ysi.pwn`. Toolchains/patches identical to exp
005/006 (native: pawn-x pawncc + `dynhook` plugin; YSI: stock pawncc from commit
461814d + `AMX_OLD_CALL`, the `PAWN_X_NO_NATIVE_FTOUCH` / `PAWN_X_NO_YSI_ITERATORS`
guards, and `#include <YSI_Internal\y_unique>` between hook bodies).
