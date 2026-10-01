# Core coverage: YSI y_va / y_iterate / y_hooks on pawn-x native

**Date:** 2026-09-28. Exhaustive map of the three YSI 5.10 core areas against
pawn-x native, read from the real `ysi-5/` source. This is the checklist for the
"adapt all YSI core natively, then rewrite the app layer on top of native"
strategy: nothing in the app layer should need a YSI-core capability that is
missing here.

Tiers used throughout:

- **native feature**: a compiler keyword or codegen (cannot be toggled by an include).
- **native library**: pawn stocks or natives that back a feature.
- **wrapper library**: pawn over host natives or over the native layer.

Gap fix tiers: **L** library-fillable, **C** contained compiler codegen (no model
change), **A** architectural (would need the stack snapshot pawn-x omits by design).

## Headline

The core capability set is closed. y_va and y_hooks have zero architectural
gaps. Every architectural gap is in the y_iterate generator layer and blocks
only the yield *sugar*, never a capability (the same iterators are writable as
manual cursor generators). Counts across the three areas: 34 (hooks) + ~113
(va, ~60 of which are the NA host-wrapper layer) + 97 (iterate).

## Closed this pass (2026-09-28, all TDD, suite 273/2)

- **va runtime-index string varargs** (native library, `<varargs>`):
  `va_strlen`, `va_getstring`, `PrintArg`. Read a string argument chosen at
  runtime by walking the caller frame, where `___(N)` needs a compile-time N.
  This was the only real y_va capability gap. va_strlen proven on pawnruns; the
  strcat/print path validated on a live open.mp server.
- **capacity-aware set queries** (native library, `<foreach>`): `setcap`,
  `setspace`, `setisfull`, `setisempty` (YSI Iter_Size / Available / IsFull /
  IsEmpty). Compiler fills `sizeof` at the call site.
- **cursor accessors** `setfirst` / `setlast` (YSI Iter_First / Last).
- **`N(count)` generator** (0..count-1), the short form of Range.
- **iterfunc array and Callback arguments + `Filter`**: an iterfunc's
  loop-invariant arguments were scalars only; they now accept a set (by-reference
  array) and a Callback predicate (`using inline`). This unlocks set-consuming
  generators and the whole predicate-iterator family. `Filter(set, using inline
  pred)` ships; Null/NonNull/Until are Filter with a specific predicate.

## y_hooks: effectively complete

All load-bearing capabilities are OK or better, zero hard gap. Return combining
is OR on a default-0 chain and AND on default-1 (matches YSI, closed
2026-09-28). pawn-x exceeds YSI on runtime handler mutation (`dynhook`), numeric
priority with no boilerplate, and JIT-safe compile-time redirection.

Remaining, not blocking:

- **C** abstract `continue` (resolve through a delegate), `HOOK_RET` with an
  expression default, long-name auto-shorten, `PRE_HOOK` priority band. Ergonomics.
- **Behavioral** ALS-macro interleaving (`hook function` mixed with legacy
  `#define` ALS in source order). Low real-world impact under the native model.
- Source-compat alias spellings (`HOOK__`, `Y_HOOKS_*_RETURN_*`) are deliberately
  skipped: the strategy rewrites on native syntax, not by compiling YSI source.

## y_va: complete bar host-wrapper layer

Core forwarding (`___`, `___(N)`) is at parity or better (no code scan, no
nesting cap, compile-time diagnostics). Closed the runtime-index string helpers
above. Deliberately skipped as source-compat sugar: `___0..9`, `va_args<>`,
`va_start<>`, the `va_*` name aliases, `GLOBAL_TAG_TYPES`. The 31 `va_*` host
wrappers and ~29 ALS redirects are the YSI library layer, rebuilt per function
when the relevant app lib is ported.

## y_iterate: core closed, catalogue and yield-sugar remain

Data ops (`set*`), multi-dim arrays, removal-safe foreach, Reverse, O(1)
`setget`, and now capacity queries + first/last are all OK or better.

Remaining by tier:

- **L (native/wrapper library, built as needed):** `Null`/`NonNull`/`Until` are
  now Filter with a specific predicate (Filter itself shipped); the random family
  (`RandomAdd`/`Remove`/`Free`, `Random(count)`); the game iterators (`Bot`/`NPC`/
  `Character`, full `Vehicle`/`Actor` tracking, the five `Streamed*`) which need
  host-event bookkeeping and belong to the entity wrapper layer built during the
  app-layer rewrite; `GetMulti` (emulable with N independent sets, losing the
  shared-value invariant).
- **C (compiler codegen):** custom `iterfunc` sentinel and seedable / multi-value
  `iterstate`; range sugar `foreach (new i : a..b)`; array parameter into a
  yield generator (the async path already copies fixed-size arrays in).
- **A (architectural, by design):** yield inside a loop/foreach and yield to
  yield delegation (error 099); local array/string and `&`ref across a yield
  (096/098). These block the yield *sugar* of the vehicle-occupant iterator
  family; the *capability* is deliverable as a manual cursor generator over
  `setnext`. The shared-value multi-iterator has no value-set analogue.

## Bottom line for the app-layer rewrite

The core capabilities the app layer leans on (forward varargs, read string args,
iterate and query sets, hook callbacks) are present natively. The open items are
either ergonomic compiler sugar (C), iterators built when their app lib is
ported (L, wrapper layer), or the yield-reach items that are architectural by
design (A) and have cursor-generator workarounds.
