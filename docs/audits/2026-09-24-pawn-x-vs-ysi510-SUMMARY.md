# pawn-x vs YSI 5.10 — Corrected Coverage Audit (2026-09-24)

**Why this exists:** every earlier pawn-x gap/parity analysis was run against a tree
named `ysi-5/` that actually held **YSI v4.00.0001**, not v5. On 2026-09-24 the vendored
tree was refreshed to the real latest **YSI v5.10.0006**, and all four native areas were
re-audited against it. This document is the corrected, honest picture. The old
"YSI-4-based" conclusions that turned out wrong are called out explicitly.

Per-area detail:
- Varargs: [2026-09-24-varargs-vs-ysi510.md](2026-09-24-varargs-vs-ysi510.md)
- Iteration + generators + yield: [2026-09-24-iteration-yield-vs-ysi510.md](2026-09-24-iteration-yield-vs-ysi510.md)
- Hooks: [2026-09-24-hooks-vs-ysi510.md](2026-09-24-hooks-vs-ysi510.md)
- Raw YSI v4→v5.10 diff: [2026-09-24-ysi4-vs-ysi510-diff.md](2026-09-24-ysi4-vs-ysi510-diff.md)

## Headline

pawn-x matches or **exceeds** real YSI 5.10 on the *core* of each area, and is genuinely
better on several axes (native C set ops, runtime hook add/remove/replace which YSI lacks,
`yield` with no stack-copy / no fixed buffer, and compiler-grade diagnostics). But against
**real** YSI 5.10 there are substantial gaps the YSI-4 comparison hid — the biggest being
that YSI 5.10 genuinely hooks functions/natives, which we had wrongly filed as a non-gap.

## Corrected gap list (most significant first)

1. **Hooks — `hook native` / `hook function` / `hook stock` call-site interception: MISSING.**
   YSI 5.10 rewrites every `SYSREQ`/`CALL` in the code segment at `OnCodeInit` (via the new
   `code-parse` + `indirection` submodules) with `continue`-based chaining. pawn-x's compiler
   `hook` is callback-only; `dynhook` intercepts `amx_Exec` public dispatch, not internal
   native/function calls. **Compiler-closable, MEDIUM** — and pawn-x is better-positioned than
   YSI (compile-time symbol rename + call redirection beats runtime AMX rewriting).
   → **Corrects the old "native-hooking is a symmetric non-gap" claim, which is now FALSE.**
2. **Yield — strict subset.** pawn-x `yield` matches YSI for linear bodies (and is mechanically
   nicer). YSI 5.10 does MORE: yield **inside a `foreach`** / delegation, yield→yield, arrays
   across a yield. pawn-x rejects these (errors 099/096/098). Consequence: YSI's new
   yield-based `VehicleOccupant`/`Passenger`/`Driver` iterators are **inexpressible** in pawn-x
   today. JIT-compat of pawn-x yield is **unverified**.
3. **Iteration — many `Iter_*` utility ops + built-in iterators MISSING.** Missing:
   RandomAdd/Remove/Free, IsFull/IsNonFull/Available (capacity-aware), GetMulti/FreeMulti,
   Begin/End/First/Last/Next/Prev, Debug, invisible iterators, and the built-ins
   Filter/Null/NonNull/Until/Bits/Blanks/None/All, Streamed*/Bot/NPC/Character. `Iterator:`
   type is PARTIAL (plain array, no tag/capacity safety, not source-compatible); `&iterstate`
   single-cell vs YSI multi-var; sentinel hardcoded to `cellmin`. Vehicle/Actor iterators are
   wrapper-only (not auto-tracked).
4. **Varargs — string-vararg runtime-index helpers MISSING.** `va_getstring`/`va_strlen`/
   `va_return`/`ReturnStringArg` need a runtime index; `___(N)` needs a constant `N`, so they
   can't be expressed today (scriptable via getarg/setarg, but absent). Also missing v5.10
   sugar: `___0..___9`, `va_args<>`/`va_start<>` aliases, `GLOBAL_TAG_TYPES`.

## What pawn-x still matches or exceeds (verified vs 5.10)

- Varargs forwarding core: nested, native+script targets, tagged varargs, numargs/getarg/setarg — FULL, better diagnostics, no macro/codescan machinery.
- Set/iteration core: Add/Remove/Contains/Count/Free/Alloc, multi-dim (incl. 3D), classic `iterfunc`, Range/RangeStep/Powers/Fib/Reverse, ready-made Player iterator — FULL (native C, no `Iter_Init` footgun).
- Yield linear bodies — FULL and better (no 512/4 caps, no stack copy).
- Hooks: multi-body callback chaining, `hook:N` priority, `hook default` return, `hook <state>`, value/ref/array arg forwarding — FULL/exceeds. **Runtime add/remove/replace (`dynhook`) — pawn-x has it, YSI does NOT.**

## Prior claims now corrected

- exp-005: "runtime add/remove/replace is the ONLY thing a compiler can't own / everything else covered" — **false**; its parity test only exercised original callback hooks, never `hook native`.
- exp-005 point 4: "YSI's hook only intercepts an existing public" — **false** for 5.10.
- exp-004 "where YSI still wins" list — **omits function/native hooking entirely.**
- exp-005 "YSI generators don't compile here (error 009)" and "YSI `foreach` + `Iter_Remove(current)` HANGS" — **YSI-4 behavior**; exp-005 `#include`d `YSI_Internal\y_unique`, a dir v5.10 dissolved, so these were never measured against 5.10. **Must be re-tested.** Note exp-005's one-script YSI-vs-native method no longer even compiles: `foreach` is now a hard pawn-x keyword that errors on YSI coexistence.
- "covers y_va" — narrow to "covers y_va **forwarding**"; the y_utils string helpers were ignored.

## Suggested next work (not yet done)

- Close the hooks gap: native `hook function`/`hook native`/`hook stock` via compile-time
  call-site redirection + a `continue` intrinsic (biggest correctness win vs 5.10).
- Extend `yield` to span a `foreach` (unlocks delegation + the VehicleOccupant family), or
  document it as a hard v1 boundary.
- Re-run a real parity harness against YSI 5.10 (the old one-script method is dead; use two
  separate builds).
- Fill the high-value `Iter_*` utility gaps and the string-vararg helpers if the north-star
  wants full-surface parity.
