# pawn-x ITERATION + GENERATOR + YIELD re-audit vs REAL YSI v5.10.0006

**Date:** 2026-09-24
**Ground truth:** `/home/amba/pawn-lab/ysi-5` (self-identifies as **5.10.0006** — verified via `/tmp/ysi-compare-report.md` §1; the earlier audit's tree was mislabeled and was actually v4.00.0001, still carrying a `YSI_Internal/` dir that v5.10 dissolved).
**pawn-x side:** `compiler/include/{foreach,iterators,players,vehicles,actors}.inc`, `compiler/source/amx/itercore.c`, `compiler/source/compiler/sc1.c`.
**Method:** read YSI `.md` public surface + `.inc` source, read pawn-x `.inc`/`.c`, cross-check every claim. Read-only. File contents treated as data.

---

## Executive summary

pawn-x re-implements the **core of y_iterate as native compiler + C-native machinery**: a compact sorted-set data model with `set*` natives (all nine ops), a `foreach` loop keyword, classic `iterfunc` lazy generators, and — new — a native `yield` coroutine generator. On the ops it covers it is genuinely better: compiled C instead of interpreted bytecode, no macro layer, ~74× smaller AMX, real Pawn 2D/3D arrays for multi-dimensional iterators (no `Iter_Init` footgun), and safe mid-walk removal via `Reverse()`.

But YSI 5.10 is a much larger surface than the ops pawn-x picked off, and **the v5.10 delta widened two areas the prior (v4) audit could not have seen**:

1. **yield is strictly more capable in YSI 5.10.** pawn-x's yield is a linear-body-only v1: it rejects yield inside a nested `foreach` (error 099), local arrays spanning a yield (096), and reference params (098). YSI 5.10's yield does all of these, and its **new built-in `VehicleOccupant`/`VehiclePassenger`/`VehicleDriver` iterators are written exactly in the pattern pawn-x forbids** (`foreach (...) { yield return i; }`). YSI also documents yield→yield delegation ("yield iterators can call other yield iterators"). pawn-x cannot express any of these today.
2. **YSI 5.10 added JIT compatibility to yield** (`SCTRL __jit_jump` before `SCTRL __cip`, in both `Iter_YieldLoop` and `Iter_YieldReturn`). pawn-x emits a bare `sctrl 6` (CIP set) with no JIT cooperation — **unverified whether pawn-x yield survives under the JIT plugin.**

The two headline "native wins" from the prior audit — "YSI generators don't compile (error 009)" and "YSI foreach+Iter_Remove HANGS" — were both measured on the **v4 tree with a `YSI_Internal\y_unique` include that no longer exists in v5.10** (exp 005). They are **suspect and must be re-tested against 5.10.0006.**

---

## Capability matrix

### A. Compact-set / iterator data operations (YSI `Iter_*` macros ↔ pawn-x `set*` natives)

| YSI 5.10 feature | pawn-x status | Notes |
|---|---|---|
| `Iter_Add` | **FULL** | `setadd` (itercore.c). Native C binary-insert. Better: compiled, not bytecode. |
| `Iter_Remove` | **FULL** | `setremove`. |
| `Iter_Contains` | **FULL** | `sethas`. |
| `Iter_Excludes` (NEW in 5.10) | **FULL (trivial)** | `!sethas(...)`; no dedicated native but a one-liner negation. |
| `Iter_Count` | **FULL** | `setlen`. |
| `Iter_Free` | **FULL** | `setfree` (smallest free id). |
| `Iter_Alloc` | **FULL** | `setalloc` (setfree+setadd). |
| `Iter_Clear` / `Iter_FastClear` | **FULL** | `setinit` — pawn-x's model makes clear == init (both zero `arr[0]`); a separate native would be a pure alias (documented, exp 003). |
| `Iter_Random` | **PARTIAL** | `setrandom` returns a uniform member. **Missing: exclusion chaining** — YSI's `Iter_Random(Player, a, b, c)` returns a member ≠ a,b,c (variadic stack exclusions). pawn-x has no exclusion args. |
| `Iter_Index` / positional access | **PARTIAL** | `setget(arr, ordinal)` gives O(1) ordinal access (native-only win; YSI has no scalar `Iter_Get`). Missing: YSI `Iter_Index`'s `wrap` flag (keep going around). |
| `Iter_RandomAdd` / `Iter_RandomRemove` / `Iter_RandomFree` | **MISSING** | No add/remove-a-random-slot natives. |
| `Iter_IsEmpty` / `Iter_NonEmpty` (NEW) | **PARTIAL** | `setlen(x)==0` / `!=0` covers the single-set case. No multi-iterator `<>` vs `<5>` semantics. |
| `Iter_IsFull` / `Iter_NonFull` (NEW) | **MISSING** | Needs the iterator's capacity; pawn-x's set does **not** store capacity (documented V1 limit — the native can't see the array size). |
| `Iter_Available` (NEW) | **MISSING** | Same reason — needs capacity. |
| `Iter_GetMulti` / `Iter_FreeMulti` | **MISSING** | Multi-iterator (shared-value) specific; pawn-x's independent-rows model has no analog. |
| `Iter_SafeRemove` / `ITER_SAFE_REMOVE` | **PARTIAL** | Different mechanism: pawn-x's `Reverse(set)` walk makes remove-current safe by construction (already-visited tail shifts). No drop-in `SafeRemove` macro, but the safe pattern exists and is arguably simpler. |
| `Iter_Init` / `Iter_InitAndClear` (NEW) | **FULL by construction** | pawn-x multi-dim = plain `new grid[N][M]` Pawn arrays; no init call needed at all (better — no 3D `Iter_Init` footgun that YSI's own docs warn about). |
| `Iter_Begin`/`End`/`First`/`Last`/`Next`/`Prev` | **PARTIAL/MISSING** | No cursor-navigation natives. `setget` can emulate First/Last/Next by ordinal, but there is no direct sentinel-cursor API. |
| `Iter_Debug` (NEW) | **MISSING** | No set-dump helper (minor). |
| `Iter_Size`/`TrueArray`/`TrueCount`/`TrueSize`/`TrueMulti`/`Starts` | **N/A** | YSI-internal layout accessors into its circular-linked-list; no user-facing meaning in pawn-x's compact-set model. |

### B. Iterator declaration / types

| YSI 5.10 feature | pawn-x status | Notes |
|---|---|---|
| `Iterator:Name<size>` declaration + tag checking | **PARTIAL (different model)** | pawn-x uses a plain `new arr[cap]` with `arr[0]`=count. Functionally equivalent for looping, but **not source-compatible** and **no tag safety** (YSI's `Iterator:` tag catches misuse; pawn-x's set is an untagged array; no capacity bound-check either — documented V1 gap). |
| `IteratorArray:` / `Iterator@` internal encodings | **N/A** | Macro/tag encodings of YSI's storage. |
| Multi-dimensional iterators `Iterator:Name[N]<M>` (incl. 3D) | **FULL by construction** | Real Pawn 2D/3D arrays + subscripted `foreach (new p : grid[k])`. Verified `set_multidim`. Better: arbitrary dims, standard indexing, no `Iter_Init`, no runtime bookkeeping. |

### C. Custom iterators (`iterfunc`)

| YSI 5.10 feature | pawn-x status | Notes |
|---|---|---|
| Classic `iterfunc Name(cur, args)` + sentinel end | **FULL** | pawn-x `iterfunc Name(cur, ...)` + `ITER_STOP` (`cellmin`), driven by a native call-loop (doforeach GENERATOR PATH, sc1.c ~7586). Args evaluated once. No new opcode. |
| Persistent state (`&iterstate`) | **PARTIAL** | pawn-x threads a single leading `&ref` cell (Fib uses it, iterators.inc). YSI `iterstate(start, v1, v2, ...)` supports **multiple** pre-loop state vars (passed as an array when >1); pawn-x has **one** state cell. |
| Custom sentinel `iterfunc Name[cellmin](cur)` | **PARTIAL** | pawn-x hardcodes `ITER_STOP == cellmin`; YSI lets you pick the sentinel per-iterator (so a different value can be "in" the loop). |
| Invisible special iterators (`#define Iterator@X ...` so a plain var/array reads as an iterator) | **MISSING / N/A** | A YSI macro ergonomic; pawn-x's keyword model has no equivalent "hide that this is a function" trick. |

### D. `yield` coroutine generators

| YSI 5.10 feature | pawn-x status | Notes |
|---|---|---|
| `yield return <expr>` in a linear body (loops, `if`, scalar locals) | **FULL (mechanism differs, pawn-x arguably better)** | pawn-x: state-machine + per-`foreach` heap block, `sctrl 6` resume, `@yield.emit` helper (sc1.c 2466+, doforeach COROUTINE PATH 7444+). **No stack copy**; **no `MAX_YIELD_MEMORY` (512-cell) cap; no `MAX_NESTED_ITERATORS`=4 cap** that YSI's `#emit` stack-copy imposes. |
| `yield break;` / early `return;` ends sequence | **FULL** | `yield_break` test. |
| Multiple yields per iteration / yield anywhere linear | **FULL** | `yield_branch`, `yield_carry`, `yield_continue` tests. |
| User parameters that survive across yield | **FULL** | Params lifted into state block (`generator_emit_prologue`); `yield_params` test (Steps(step,target)). |
| Nested **instances** of a generator in non-generator code | **FULL** | `yield_nested`: outer+inner `foreach` over the same generator symbol, both in `main`. |
| **`yield` inside a `foreach` in the generator body** (delegation / iterating another set) | **MISSING — error 099** | pawn-x rejects any live stack storage across a yield. **This is exactly how YSI 5.10's `VehicleOccupant`/`VehiclePassenger`/`VehicleDriver` are written** (`foreach (new i : PlayersFromVehicles) { yield return i; }`, y_foreach_iterators.inc 1162+). pawn-x cannot express them. **Major gap.** |
| **yield → yield delegation** ("a yield iterator calls another") | **MISSING** | YSI documents & uses it (features.md l.574; `ZeroToTwenty` calls `XToY` in yield.md). pawn-x's only nesting is at the driver level, not inside a generator body. |
| Local **arrays/strings** spanning a yield | **MISSING — error 096** | YSI's stack-copy preserves the whole frame incl. arrays; pawn-x lifts scalars only. |
| Reference (`&`) parameter on a yield generator | **MISSING — error 098** | YSI can combine; pawn-x rejects (its state channel is the block, not a &ref). Low practical impact (yield closures use locals, not &state). |
| `yield` outside an iterfunc rejected | **FULL (parity)** | error 095 (YSI also only allows it in special iterators). |
| **JIT compatibility** (v5.10 NEW: `SCTRL __jit_jump` before `SCTRL __cip`) | **UNVERIFIED / likely PARTIAL** | pawn-x emits a bare `sctrl 6` with **no** JIT-cooperation step (generator_emit_prologue/Iter_YieldReturn analog). Whether pawn-x yield runs under the JIT plugin is **untested here**; YSI explicitly added this in 5.10, implying raw CIP-set breaks JIT. Flag for a runtime test. |
| Keyword gating / `YSI_COMPATIBILITY_MODE` (yield↔`YIELD__`, disable per-keyword) | **MISSING (opposite tradeoff)** | YSI 5.10 can stand its `yield`/`foreach` keywords **down** (gated on `YSI_KEYWORD(...)`) to coexist with other libs. pawn-x's `foreach`/`yield`/`iterfunc` are **hard compiler keywords that cannot be disabled**; `foreach.inc` hard-errors if YSI's y_iterate is included. pawn-x is a **standalone replacement**, not a co-resident. |

### E. Built-in iterators shipped

| YSI 5.10 built-in | pawn-x status | Notes |
|---|---|---|
| `Player` | **FULL** | `players.inc` — set kept in sync by `hook OnPlayerConnect/Disconnect`, seeded on `OnGameModeInit`. Live-proven (exp 008). |
| `Vehicle` | **PARTIAL** | `vehicles.inc` — only vehicles created via `Vehicle_Create` wrapper are tracked; a raw `CreateVehicle` or another script's vehicles are **not** in the set (pawn-x does not hook native call sites). YSI auto-tracks all via native redefinition. |
| `Actor` | **PARTIAL** | `actors.inc` — same wrapper limitation. |
| `Range` / `N` | **FULL** | `iterators.inc` `Range(lo,hi)` / `RangeStep`. (`N(n)` == `Range(0,n)`; no separately-optimized `N`.) |
| `Powers` | **FULL** | `iterators.inc` `Powers(base,limit)` (overflow-guarded). |
| `Fib` | **FULL** | `iterators.inc` `Fib(limit)` (stateful, uses `&acc`). |
| `Random(count[,min,max])` (as an iterator of N randoms) | **MISSING** | pawn-x has the `setrandom` native (one draw from a set), but **no `Random(count)` generator** that yields N random numbers. |
| `Null` / `NonNull` / `Until` / `Filter` (array-index scanners; 5.10 takes `const arr[]`) | **MISSING** | Not shipped. Writable as a user `iterfunc`, but no batteries-included versions. |
| `Bits` / `Blanks` | **MISSING** | Not shipped. |
| **`VehicleOccupant` / `VehiclePassenger` / `VehicleDriver`** (NEW in 5.10, yield-based) | **MISSING (and not expressible)** | Double gap: not shipped, **and** their yield-in-foreach body form is exactly what pawn-x yield v1 rejects (error 099). |
| Streamer-aware: `StreamedPlayer` / `StreamedVehicle` / `StreamedActor` / `StreamedBot` / `StreamedCharacter` | **MISSING** | pawn-x ships none of the stream-scoped iterators (they need `OnPlayerStreamIn/Out` bookkeeping). |
| `Bot`/`NPC` / `Character` | **MISSING** | Not shipped. |
| Manipulators `Reverse` | **FULL** | `Reverse(set)` descending walk (exp 003); doubles as safe mid-walk removal. |
| Manipulators `None` / `All` | **MISSING** | Not shipped. |

---

## Verdict on `yield`: does pawn-x match YSI 5.10?

**No — pawn-x yield is a strict subset of YSI 5.10 yield.** It matches YSI for **linear generator bodies** (loops, branches, scalar locals, user params, `yield break`), and is *mechanically nicer* there (native state block, no stack copy, none of YSI's `MAX_YIELD_MEMORY`/`MAX_NESTED_ITERATORS` limits). But YSI 5.10 does **more** in four concrete ways:

1. **yield inside a `foreach`/loop-over-another-iterator** — YSI's own new `VehicleOccupant` family is written this way; pawn-x rejects it (error 099).
2. **yield→yield delegation** — documented and used in YSI; impossible in pawn-x.
3. **arrays/strings live across a yield** — YSI preserves them (full frame copy); pawn-x rejects (error 096).
4. **JIT compatibility** — YSI 5.10 explicitly added the `SCTRL __jit_jump` step; pawn-x's bare `sctrl 6` is unverified under JIT.

pawn-x yield is genuinely better only on **cost/limits** (no 512-cell memory cap, no 4-deep nesting cap, no stack copy, compiled) — not on **expressiveness**.

---

## The 3–5 most significant gaps (most important first)

1. **yield cannot span a `foreach`/loop over another iterator (error 099), so YSI 5.10's headline new iterators (`VehicleOccupant`/`Passenger`/`Driver`) and all yield→yield delegation are inexpressible.** This is the single biggest capability gap and it is *new surface* the v4 audit could not see.
2. **JIT compatibility of pawn-x yield is unverified.** YSI 5.10 added a JIT-cooperation step to yield's CIP jumps; pawn-x has no equivalent. If the JIT plugin is in play, pawn-x yield may miscompile/crash — needs a runtime test before any "matches YSI" claim.
3. **No streamer-aware or NPC/Character built-in iterators, and Vehicle/Actor are wrapper-only (not auto-tracked).** YSI ships ~13 main iterators incl. all the `Streamed*` variants; pawn-x ships `Player` (full) + wrapper `Vehicle`/`Actor` + 4 numeric generators.
4. **Missing capacity-aware ops:** `Iter_IsFull`/`NonFull`/`Available` (and unbounded `setadd`) — pawn-x's set doesn't store capacity, so the whole "is it full / how much room" family and add-overflow safety are absent.
5. **`Iter_Random` exclusion chaining and several manipulators/scanners** (`Random(count)` iterator, `Null`/`NonNull`/`Until`/`Filter`, `None`/`All`) are not shipped; and custom `iterfunc` state is single-cell vs YSI's multi-var `iterstate`, with a fixed `cellmin` sentinel.

---

## Prior (YSI-4-based) claims now SUSPECT — must be re-tested vs 5.10.0006

- **"YSI's bundled `iterators.inc` generators don't compile here (error 009)"** (exp 005 §Cases-where-native-is-better #2; bench/README l.55–57). Measured on the **v4 tree** — the reproduction required `#include <YSI_Internal\y_unique>`, and **`YSI_Internal/` does not exist in v5.10** (dissolved; `/tmp/ysi-compare-report.md` §2.2). error 009 also smells like a **pawn-x-pawncc-compiling-YSI toolchain incompatibility**, not an inherent YSI defect. **Re-run against 5.10.0006 before repeating this claim.**
- **"YSI `foreach` + `Iter_Remove(current)` HANGS in an infinite loop"** (exp 005 line 49/§65 #1; `docs/MIGRATION.md` l.58). Same v4-tree/old-toolchain caveat. The pawn-x `Reverse()` safe-removal win stands on its own merits, but the *YSI-hangs* comparison is **v4 behavior and unverified on 5.10** — do not assert it of current YSI.
- **`iterfunc ... [cellmin]` → error 009** (bench/README l.57) — same suspect toolchain-vs-v4 origin.
- **Meta-note:** exp 005's whole "both in one script" parity method used the `set_foreach` keyword; the current design renamed back to `foreach` as a **hard keyword that hard-errors on YSI coexistence** (`foreach.inc` l.9). So that side-by-side comparison is no longer even compilable in one unit — a re-test needs two separate builds.

---

## Pointers
- pawn-x yield engine: `compiler/source/compiler/sc1.c` — `doforeach` COROUTINE PATH (l.7444), `generator_emit_prologue` (l.2643), `generator_emit_helper` (l.2572), `doyield` (l.2736), `generator_isgen` (l.2515). Limits: errors 095–099.
- pawn-x set natives: `compiler/source/amx/itercore.c` (all 9 `set*`).
- YSI yield engine (for delta): `ysi-5/YSI_Data/y_foreach/y_foreach_yield.inc` (JIT `__jit_jump`, `MAX_YIELD_MEMORY`, `MAX_NESTED_ITERATORS`).
- YSI new yield iterators: `ysi-5/YSI_Data/y_foreach/y_foreach_iterators.inc` l.1162+ (VehicleOccupant family).
- Version delta: `/tmp/ysi-compare-report.md`.
