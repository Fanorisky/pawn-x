# pawn-x gap analysis — where the native features do NOT yet fully close the gap

**Date:** 2026-09-26 · **HEAD:** `b7e8f92` (main, async + switch-codegen merged) ·
**Compiler:** `build/pawncc` 3.10.11

**Scope:** every YSI-replacement area (`y_va`, `y_iterate`/`y_foreach`, `y_hooks`)
plus native `async`/`await` vs **PawnPlus**. YSI never shipped working async, so
PawnPlus is the async baseline.

**Method:** four parallel audits, each read the actual vendored source
(`ysi-5/` = real YSI 5.10.0006, `compiler/include/`, `compiler/source/compiler/`,
`compiler/source/amx/itercore.c`) and, where possible, compiled/ran probes against
the current `build/pawncc` + `build/pawnruns`. This supersedes the
`docs/audits/2026-09-24-*` set (now stale: hooks audit predates the 2026-09-25
variadic/`numargs` closure; all line numbers shifted after the async merge).

**Legend:** ✅ closed (parity or better) · ◑ partial / different model · ✗ gap ·
N/A not applicable to pawn-x's design.

## Executive summary

The **core** of every area is closed, and pawn-x **exceeds** YSI in several places
(runtime hook mutation, priority without include tricks, O(1) `setget`, yield
without a stack copy, compile-time diagnostics, ~19× smaller output, no bytecode
surgery). The remaining gaps, by theme:

1. **Coroutine reach (yield + async).** The biggest expressiveness gap. `yield`
   can't cross a `foreach`/loop or delegate (error 099), which makes YSI 5.10's
   `VehicleOccupant`/`Passenger`/`Driver` iterators inexpressible; async can't
   suspend at arbitrary call-stack depth or snapshot a whole frame (PawnPlus can).
   Both trace to the same root: pawn-x lifts only a coroutine's own locals, it
   never snapshots the stack. **Architectural**, by deliberate design.
2. **Runtime-index string varargs.** `va_getstring`/`va_strlen`/`va_return` take a
   runtime index; `___(N)` needs a compile-time constant. Implementable in pawn
   over `getarg`, but not shipped.
3. **Capacity-aware iteration + the wider iterator/task catalogue.** `Iter_IsFull`/
   `Available`, streamer/NPC iterators, task cancellation/timeout/reuse — mostly
   library-fillable, a few architectural.
4. **JIT compatibility of yield/async is unverified** (no JIT plugin in-repo).

Full per-area tables and an impact-ordered master list follow.

---

## 1. Varargs — `y_va` vs `___`

`___` is a **compiler** token (recognised in argument position, `sc3.c` `matchfwdtoken`)
lowered to a runtime copy loop + dynamic byte count + native-call cleanup. YSI's `___`
is a preprocessor macro resolved by a runtime bytecode codescan (`OnCodeInit` +
amx_assembly/code-parse/indirection submodules). Core forwarding is at parity; the
gaps are runtime-index string helpers and sugar.

| y_va feature | what it does | pawn-x | status |
|---|---|---|---|
| bare `___` | forward the whole variadic tail | compiler token, skips enclosing named params | ◑ (semantics differ from YSI bare-`___`, see note) |
| `___(N)` | forward skipping first N (constant) | `___(N)`, constant N only | ✅ |
| `___0`…`___9` | shorthand for `___(0..9)` | none (`___5` lexes as a plain identifier) | ✗ sugar |
| `va_args<T>` / `va_start<N>` | aliases for tagged-varargs / `___(N)` | none (use native `{tags}:...` / `___(N)`) | ◑ alias-only |
| nested / into-native / into-pawn-fn forwarding | dynamic passthrough | `fwdpushloop`/`fwdbytecount`/`fwdpopnative`, no fixed cap (YSI caps at 4) | ✅ (exceeds) |
| `numargs` / `getarg` / `setarg` | runtime arg introspection | native (`core.inc:16-18`) | ✅ |
| `va_strlen(idx)` / `va_getstring(dest,idx,len)` | strlen / getarg for a string vararg by **runtime** index | none | ✗ **gap** (`___(N)` needs constant N) |
| `ReturnStringArg` / `va_return(fmt,...)` / `PrintArg(n)` | return / print a string vararg by runtime index | none | ✗ gap |
| `GLOBAL_TAG_TYPES` / `CUSTOM_TAG_TYPES` | predefined / extensible multi-tag set | none (native tagged varargs exist, no preset) | ✗ minor |
| ~30 auto-extended stdlib fns + `va_*` wrappers | SendClientMessage/format wrappers over host natives | none | N/A (YSI library layer, not a language feature) |

**Top varargs gaps:** (1) **runtime-index string helpers** (`va_getstring`/`va_strlen`/
`va_return`/`PrintArg`) — the only real capability loss; scriptable over `getarg` but
not shipped. (2) `GLOBAL_TAG_TYPES`/`CUSTOM_TAG_TYPES` presets. (3) `___0..9` / `va_args<>`
/ `va_start<>` sugar (zero capability lost, closable with a preprocessor shim).

**pawn-x exceeds:** compile-time diagnostics (error 253 `___` without `...`, 254 `___`
in a type-checked slot, 58 two `___` in one call), `___` still usable as an ordinary
identifier, runtime clamp when `___(N)` overshoots, no codescan/submodule/`OnCodeInit`
dependency.

**Note (unverified):** YSI bare `___` expands with `skippedBytes=0` (skip nothing) then
adjusts in the codescan; pawn-x bare `___` skips enclosing named params. Possible
observable divergence for a truly-bare `___` after named params; for the idiomatic
`___(N)` form the two are identical. Idiomatic YSI always writes `___(N)`.

---

## 2. Iteration + Generators — `y_iterate`/`y_foreach` vs `foreach` + `set*` + `iterfunc`/`yield`

pawn-x uses a **value-set** model (a plain sorted array is the set) with 9 native
`set*` ops (`itercore.c`) and a compiler `foreach` keyword; YSI uses a tagged
circular-list `Iterator:`. Data ops are largely at parity; the real gaps are
capacity-awareness, the wider iterator catalogue, and (biggest) yield reach.

### 2a. Data ops (`Iter_*` ↔ `set*`)

| YSI `Iter_*` | pawn-x | status |
|---|---|---|
| `Add` / `Remove` / `Contains` / `Count` / `Free` / `Alloc` / `Clear`/`Init` | `setadd`/`setremove`/`sethas`/`setlen`/`setfree`/`setalloc`/`setinit` | ✅ |
| `Index` (nth member) | `setget` ordinal, O(1) native | ◑ (no `wrap` flag; O(1) is a win) |
| `Random` (+ variadic exclusions) | `setrandom` | ◑ (no exclusion-chaining) |
| `SafeRemove` / `ITER_SAFE_REMOVE` | `Reverse(set)` descending walk (safe-by-construction) | ◑ (different mechanism, no drop-in macro) |
| `IsFull` / `NonFull` / `Available` | none — the set stores no capacity | ✗ **gap** |
| `RandomAdd` / `RandomRemove` / `RandomFree` | none | ✗ gap |
| `GetMulti` / `FreeMulti` / `Multi` / `Single` (shared-value multi-iterators) | none (independent-array model) | ✗ gap |
| `Begin`/`End`/`First`/`Last`/`Next`/`Prev` cursor nav | `setget` by ordinal only | ◑ (no sentinel-cursor API) |
| `Debug` | none | ✗ minor |
| `Iterator:Name<cap>` tag + capacity/bounds safety | plain `new arr[cap]` (`arr[0]`=count) | ◑ (untagged, not source-compatible, no bounds safety) |
| multi-dim `Iterator:Name[N]<M>` (incl 3D) | real Pawn 2D/3D arrays + `foreach(:grid[k])` | ✅ (no `Iter_Init` footgun) |
| `Size`/`TrueArray`/`TrueCount`/`Starts`/`State`/`Tag` internal accessors | none | N/A (YSI circular-list internals) |

Unbounded `setadd` (no capacity bound-check) is the same root as the missing
`IsFull`/`Available`: the native cannot see array capacity (documented V1 limit).

### 2b. Generators (`iterfunc`) and coroutines (`yield`)

All yield limits below were confirmed at runtime (probes compiled + run).

| YSI feature | pawn-x | status |
|---|---|---|
| classic `iterfunc(cur,args)` + sentinel | `iterfunc` + `ITER_STOP==cellmin` | ✅ (`Range`/`Powers` verified) |
| persistent state | single leading `&ref` cell (`Fib`) | ◑ (one cell, init 0; YSI `iterstate` sets seed + multiple state) |
| custom sentinel `iterfunc Name[cellmin](cur)` | none (hardcoded `cellmin`) | ◑ |
| invisible iterators (`#define Iterator@X`) | none | ✗ / N/A (keyword model, no macro-hiding) |
| `yield return expr` in a **linear** body | state-machine + heap block, `@yield.emit`, bare `sctrl 6` | ✅ (ran: `Count(5)`→10; no YSI 512-cell / 4-nest caps) |
| `yield break;` / bare `return;` | supported | ✅ |
| multiple yields / scalar params across yield | lifted into block | ✅ |
| **yield inside a `foreach`/loop** in the gen body | rejected **error 099** (verified) | ✗ **gap** (blocks YSI `VehicleOccupant`/`Passenger`/`Driver`) |
| **yield → yield delegation** | rejected error 099 (verified) | ✗ gap |
| local **array/string** across yield | rejected **error 096** (verified) | ✗ gap |
| **`&`ref param** on a yield gen | rejected **error 098** (verified) | ✗ gap |
| **array param** on a yield gen | rejected error 255 | ✗ gap (note: the *async* path DOES copy-in fixed-size array params — machinery partly exists, not extended to yield) |
| yield-outside-iterfunc / yield-as-expression rejects | error 095 / 097 | ✅ parity |
| **JIT compatibility** | emits bare `sctrl 6` only; YSI also emits `sctrl 8` (`__jit_jump`) | ◑ **unverified / mechanically suspect** |

### 2c. Built-in iterators shipped

`Player` ✅ (auto-tracked). `Vehicle`/`Actor` ◑ (wrapper-tracked only — raw
`CreateVehicle` untracked). `Range`/`N`/`Powers`/`Fib` ✅. `Reverse` ✅.
**Missing (✗):** `Random(count)`, `Null`/`NonNull`/`Until`/`Filter` (writable as user
`iterfunc`), `VehicleOccupant`/`Passenger`/`Driver` (✗ **inexpressible** — needs
yield-in-foreach), all `Streamed{Player,Vehicle,Actor,Bot,Character}`, `Bot`/`NPC`/
`Character`, manipulators `None`/`All`.

**Top iteration/generator gaps:** (1) **yield can't cross a loop/foreach or delegate
(099)** — biggest expressiveness gap, blocks the whole v5.10 vehicle-occupant family.
(2) **yield JIT-compat unverified** (bare `sctrl 6`, no `sctrl 8`). (3) **no streamer/
NPC iterators; Vehicle/Actor wrapper-only.** (4) **arrays/refs can't span/enter a yield
gen** (096/098/255) — async already copies fixed arrays in, not extended to yield.
(5) **capacity-aware ops absent** (`IsFull`/`Available`, unbounded `setadd`).
(6) assorted: `Iter_Random` exclusions, `RandomAdd/Remove/Free`, `GetMulti`, the
`Random`/`Null`/`Filter`/`Until` iterators, multi-value `iterstate` + custom sentinel.

---

## 3. Hooks — `y_hooks` vs `hook` keyword + call-site hooks + `dynhook`

The 2026-09-24 hooks audit is **obsolete**: its headline gaps (call-site
`hook native`/`function`/`stock` + `continue`, and variadic + `numargs`-in-body) were
closed by exp 010/011 and are merged. The audit agent re-confirmed by compiling and
running probes (`continue(base, ___)`→116; `numargs()` in a hook body = 4 user args
with the hidden idx invisible; `hook native random` clamps via `continue`; `hook:10`
priority; wrong-modifier rejected error 258).

| y_hooks feature | pawn-x | status |
|---|---|---|
| callback stacking (many bodies / one callback) | `hook` keyword → hidden bodies + synthesised dispatcher, no `y_unique` boilerplate | ✅ (exceeds) |
| chain control `~0`/`~1` + continue-with-0/1 | `HOOK_STOP`/`HOOK_STOP_1`/`HOOK_CONTINUE`/`HOOK_CONTINUE_0` | ✅ |
| `HOOK_RET` default return | `hook default Name = k;` | ✅ |
| state-scoped hooks `<state>` | `hook <state>` / `<automaton:state>` runtime gate | ✅ |
| priority / order (`PRE_HOOK`/`@N`/CHAIN_ORDER) | `hook:N` numeric, stable sort | ✅ (exceeds: no name-suffix + re-include dance) |
| **call-site `hook native`/`function`/`stock`** + `continue` | compile-time call redirection to wrapper+chain; original endpoint real SYSREQ/`call` | ✅ (was Gap A/B) |
| forward references (call before hook decl) | persistent seen-set + one forced reparse, size-stable | ✅ |
| **variadic call-hooks + `numargs`/`getarg`/`setarg` in body** | shape checked; runtime tail copy; idx hidden; `continue(fixed, ___)` / bare `continue()` | ✅ (verified by run) |
| runtime add/remove/replace of handlers | `dynhook` plugin (`dynhook_add/remove/replace/clear` + transparent `intercept`) | ✅ **exceeds** (YSI has no runtime handler API) |
| `DEFINE_HOOK_REPLACEMENT__` long-name auto-shorten | none (different hidden-name scheme) | ◑ N/A (extreme names can still hit error 200) |
| **ALS-macro interleaving** (mix `hook function` with `#define` ALS in source order) | none — a `#define Foo My_Foo` retargets call sites away from the chain | ◑ **partial gap** |
| cross-`.amx` hooking | none (host dispatches to every script) | N/A (out of scope both sides) |

**Top hook gaps:** (1) **ALS-macro interleaving** (◑) — the one genuine behavioral
difference; low real-world impact under the north-star (use `hook function` + `hook:N`),
not compiler-trivial to close. (2) **long-name auto-shorten** (◑, cosmetic edge — extreme
names trip error 200). (3) `dynhook` intercepts the `amx_Exec` public boundary, not
runtime native SYSREQ sites (neither does YSI).

**pawn-x exceeds:** runtime handler mutation (YSI has none), priority without include
tricks, no per-body boilerplate + smaller output, compile-time call redirection is
JIT-safe with zero startup code-scan (YSI rewrites the whole code segment at `OnCodeInit`).

---

## 4. Async / await — pawn-x native vs PawnPlus

> **UPDATE 2026-09-27 (`feat/async-headroom`):** the *library-fillable* async gaps
> below are now CLOSED — no model change, all TDD'd + suite-green (240/2):
> - **task_keep / result** — `Async_Keep` / `Async_Result` / `Async_HasResult` /
>   `Async_Done` / `Async_Release` (a small codegen add captures a top-level
>   coroutine's return value at completion). Test `async_keep`.
> - **cancellation (task_delete)** — `Async_Cancel` (cooperative, fault-notified via
>   `Async_Cancelled()`) + `Async_Kill` (hard). Test `async_cancel`.
> - **task_bind / task_detach** — `Async_Bind` / `Async_Detach` completion callbacks
>   (host dispatch via `CallLocalFunction` under `ASYNC_HOST_CALLBACKS`). Test `async_bind`.
> - **timeout (task_set_error_ms)** — `Async_Timeout` / `Async_TimeoutGate` (host
>   `SetTimerEx`); race-free gate-timeout logic tested off-host (`async_timeout`).
> - **implicit fault auto-raise** — an unobserved leaf fault now propagates to its
>   awaiter instead of dropping. Test `async_fault_autoraise`.
>
> What REMAINS is the *architectural* set (A2/A3 below): no whole-frame snapshot, so
> no arbitrary-depth suspend, no await in a variadic call's args, no composed-then-leaf,
> no unsized/multi-dim/&ref params across await; plus cross-`.amx` and JIT-compat. The
> tables below are the original audit snapshot at HEAD `b7e8f92`.

pawn-x async is a **compile-time coroutine transform** that lifts only the coroutine's
own locals/params into a size-classed data-segment arena (no stack/heap to save) +
an in-script scheduler (`async.inc`) + an open.mp timer adapter (`async_omp.inc`).
PawnPlus is a **runtime plugin** whose `await` default (`task_restore_full`) `memcpy`s
the entire AMX stack+heap and re-enters `amx_Exec`. That single difference is the root
of nearly every gap below.

### 4a. Async / task features

| PawnPlus feature | pawn-x native | status |
|---|---|---|
| `await` (task_await) | `await expr` | ✅ (scalar result) |
| `task_set_result` (complete with value) | `Async_Resume(token,value)` (the host seam) | ✅ |
| `task_ms`/`task_ticks`/`wait_ms` | `await Async_Ms(ms)` / `Async_Ticks(n)` | ✅ (validated live open.mp 1.5.8) |
| `task_all` / `task_any` | `Async_All`/`Async_Any` + `await Async_Wait` + `Async_GateFeed` | ✅ (all = sum, any = first) |
| `task_set_error` (fault) | `Async_ResumeError` / `Async_GateFail` | ✅ |
| faulted result **raises** an AMX error up the chain | post-await `Async_Failed()`/`Async_Error()` + `Async_Fail(err)` auto-raise up a *composed* chain | ◑ (explicit flag-check, not exception-unwind; an *ignored* leaf fault does not auto-raise) |
| `await_arr` / multi-value result | `Async_ResumeArr` + `Async_InboxArr` | ✅ (clamped to `ASYNC_RESULT_MAX`) |
| first-class `task_new()` object | `Async_Start(Fn,args)`→token; you await an async *function*, not a task value | ◑ |
| `task_set_error_ms` (timeout / delayed error) | none built-in | ✗ (compose a timer → `Async_ResumeError`) |
| `task_delete` / cancellation | `Async_Free` frees a slot, no cancel-and-notify | ✗ |
| `task_keep` / `task_reset` (keep/reuse result) | none — free-on-complete | ✗ |
| `task_config(restore_none/frame/context/full)` | none — always "lifted locals only" | ✗ **architectural** |
| suspend at arbitrary call-stack depth (await in a non-async helper) | rejected error 265 | ✗ **architectural** |
| `await` inside a **variadic** call's args | rejected error 099 (fixed-arity call-arg await ✅) | ✗ |
| **multiple** awaits in one expression | leaf+leaf, 3-in-a-row, ternary, compose+compose, leaf-then-compose ✅ (verified 9/6/111/203/17); only **composed-then-leaf** rejected 099 | ◑ (one shape short) |
| unsized/multi-dim/`&`ref **params** across await | rejected error 268; *fixed-size* array/string params copied in ✅ | ◑ |
| `task_bind`/`task_detach` dynamic callback binding | manual `public`+`Async_Resume`, or `hook`/`dynhook` | ◑ (no format-spec binding) |
| cross-`.amx` await | single `.amx` only | ✗ |
| JIT compatibility | unverified | ✗ (open) |

### 4b. Broader PawnPlus runtime that async code often leans on (pawn-x has no equivalent)

| PawnPlus | why it matters for async | pawn-x |
|---|---|---|
| dynamic strings (`str_*`) | hold an await result of unknown length (query rows, HTTP bodies) | ✗ fixed cell arrays only |
| dynamic containers (`list`/`linked_list`/`map` + iterators) | queue pending tasks, map ids→results | ✗ static value-sets only |
| variants (`var`) | a task result "of any type/tag" | ✗ single-cell / fixed-shape |
| GC + `handle_*` / `pawn_guard` | manage objects that outlive a suspend | ✗ static arena, slot free-on-complete |
| `pawn_*` expressions / reflection / dynamic invocation | build/bind handlers at runtime | ✗ compile-time only |

pawn-x deliberately trades these for a plugin-free, static, compile-time design.

**Top async gaps vs PawnPlus (most impactful first):**
1. **No whole-frame snapshot (`task_config`/`restore_full`)** — *architectural*, the root
   of the next three: no arbitrary-depth suspend, no await in variadic-call args, no
   composed-then-leaf multi-await. Closing it means the frame-snapshot approach pawn-x
   explicitly rejected (design spec §3). Not fixable without abandoning the leaner model.
2. **No cancellation / timeout** (`task_delete`, `task_set_error_ms`) — *library-fixable*
   over the existing `Async_ResumeError` seam.
3. **No reusable/kept tasks / first-class task objects** (`task_keep`/`reset`/`wait`) —
   *fixable* but a model shift (await functions, not task values).
4. **No error-as-exception propagation** — *partly fixable*; an ignored leaf fault silently
   drops. Making leaf faults auto-raise is a library/codegen decision.
5. **Unsized/multi-dim/by-ref params across await** — *architectural-adjacent*; fixed-size
   copy-in works, general case needs frame aliasing (or wider documented copy-in).
6. **No dynamic containers/strings/variants/GC** — *large separate features*; matters when
   results are variable-size or must be queued.
7. **`task_bind`/`task_detach`** dynamic binding — *fixable* via the hook pillar + wrapper.
8. **cross-`.amx` await** and **JIT compat** — roadmap/non-goal; JIT genuinely unverified.

---

## 5. Master list — remaining gaps by impact

**Architectural (by design — would require abandoning the plugin-free / no-stack-snapshot model):**
- A1. yield can't cross a `foreach`/loop or delegate yield→yield (error 099) — blocks YSI 5.10 `VehicleOccupant`/`Passenger`/`Driver`.
- A2. async can't suspend at arbitrary call-stack depth / no whole-frame snapshot (`task_config`) — blocks await-in-variadic-call, composed-then-leaf, await in a non-async helper.
- A3. arrays/strings/`&`ref across yield (096/098/255); unsized/multi-dim/ref params across await (268). Fixed-size copy-in exists on the async path; extending it to yield and to more shapes is *partly* fixable, the general case is architectural.
- A4. cross-`.amx` (yield/async/hooks) — out of scope.

**Fixable (library or contained codegen, no model change):**
- F1. runtime-index string varargs: `va_getstring`/`va_strlen`/`va_return`/`PrintArg` (over `getarg`).
- F2. capacity-aware iteration: `Iter_IsFull`/`NonFull`/`Available` + bounded `setadd` (needs the set to carry capacity).
- F3. async cancellation + timeout (`task_delete`, `task_set_error_ms`) over the `Async_ResumeError` seam; reusable/kept tasks; leaf-fault auto-raise.
- F4. missing iterators (`Random(count)`, `Null`/`NonNull`/`Until`/`Filter`, `None`/`All`) — writable as user `iterfunc`; streamer/NPC iterators need `OnPlayerStreamIn/Out` bookkeeping; `Vehicle`/`Actor` auto-tracking beyond wrappers.
- F5. iteration ops: `Iter_Random` exclusions, `RandomAdd/Remove/Free`, `GetMulti`, cursor nav (`Begin`/`End`/`Next`/`Prev`), multi-value `iterstate` + custom `iterfunc` sentinel.
- F6. sugar/aliases: `___0..9`, `va_args<>`, `va_start<>`, `GLOBAL_TAG_TYPES`/`CUSTOM_TAG_TYPES`.
- F7. `task_bind`/`task_detach` dynamic callback binding via the hook pillar.

**Unverified (need a JIT plugin / live host, not present in-repo):**
- U1. yield JIT compatibility — pawn-x emits bare `sctrl 6`, YSI also emits `sctrl 8` (`__jit_jump`). Runs on the interpreter; JIT behavior unknown.
- U2. async JIT compatibility and cross-`.amx` — genuinely unverified.
- U3. bare-`___` vs `___(N)` semantic divergence when named params precede `...` (idiomatic `___(N)` is identical).

**Behavioral difference (not a capability gap):**
- B1. hooks: ALS-macro interleaving (`hook function` + `#define` ALS in source order) — pawn-x can't interleave with legacy macro-ALS.
- B2. long callback names can trip error 200 (no auto-shorten). Cosmetic.

## 6. Corrections found while auditing

- **Stale repo doc / misleading demo:** `experiments/012-native-async/comparison/gap_native_reject.pwn`, `combo_gap.pwn`, and `HOST-VALIDATION.md:80-114` present `await Async_Ms(150) + await Async_Ms(150)` (leaf+leaf) as the "PawnPlus-only" rejected gap. **It is no longer rejected** — leaf+leaf multi-await now compiles (confirmed by the `async_multiawait` test). The genuinely rejected shape is **composed-then-leaf** (`await Inner() + await Async_Pending()` → error 099). The demo predates the multi-await generalization and should be updated to the composed-then-leaf example.
- The `docs/audits/2026-09-24-*` set is superseded: line numbers shifted after the async merge, and the hooks audit predates the 2026-09-25 variadic/`numargs` closure.

## 7. Bottom line

pawn-x meets or beats YSI on the **core** of every replaced area and adds capabilities
YSI lacks (runtime hook mutation, cleaner priority, O(1) `setget`, plugin-free async).
The durable, by-design gaps are the **coroutine reach** items (A1–A3): pawn-x lifts only
a coroutine's own locals and never snapshots the stack, so anything needing a live frame
across a suspend (yield-in-loop, deep async suspend, arbitrary frame temporaries) stays
out of reach without changing the model. Everything else is either library-fillable
(varargs string helpers, capacity ops, task cancellation, missing iterators) or unverified
pending a JIT/host run (U1–U2).







