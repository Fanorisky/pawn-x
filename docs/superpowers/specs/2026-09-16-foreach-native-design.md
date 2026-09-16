# Native `foreach` & Iterator API — Experiment 002 Design

**Status:** Design (in-session review, 2026-09-16)
**Experiment:** pawn-x experiment 002
**Upstream:** openmultiplayer/compiler (vendored at `compiler/`)
**Replaces:** YSI `y_foreach` / `y_iterate` macro-iteration + iterator API

## 1. Problem

Pawn has no first-class set-collection. To iterate over a dynamic
collection of integers, script authors depend on YSI `y_foreach`, which
implements `Iterator` + `foreach (new i : X) { ... }` entirely as
macros over a circular sorted linked-list array (the `F@`/`Y_FOREACH_`
obfuscated macro layers in `ysi-5/YSI_Data/y_foreach/`), plus a battery
of `Iter_Add/Remove/Contains/Free` stock functions.

The compiler knows nothing about any of it. There is no `foreach`
keyword, no iterator type, no set-semantics — the whole layer is script
side. This feature moves it into the compiler: a real loop keyword with
codegen, a plain-symbol data layout the compiler maintains, and native
`Iter_*` functions.

## 2. Feature

```pawn
new MySet[128];                 // plain array, compiler-managed set
Iter_Init(MySet, 128);         // initialise empty (or auto on first use)

Iter_Add(MySet, 42);
Iter_Add(MySet, 7);

foreach (new i : MySet)        // iterate the *values* present, sorted
{
	printf("have %d\n", i);
}                               // prints 7 then 42
```

`foreach` is a compiler keyword that compiles to a generated loop over
the in-use values of a plain array, backed by the `Iter_*` natives.
The loop variable (`i`) is scoped to the loop body.

## 3. Decisions (locked in-session)

| Aspect | Decision | Rationale |
|---|---|---|
| Data layout | **Own compact sorted array**, NOT the YSI circular-list. In-use items occupy slots `0..count-1` in ascending order; the set is a sorted run of distinct non-negative values. | Chosen "simpler"; scan is cache-friendly and trivially codegen-able; decouples from YSI's pointer-chasing structure |
| Declaration | **Plain symbol** — user declares `new X[size]`; no `Iterator` tag, no auto-expand. | Simplest; no tag machinery; `foreach` and `Iter_*` just take the array name + size |
| In-use encoding | Compiler keeps a **parallel count** and a **sorted compact region** `[0..count)`. A value is present iff it is in the compact run. | No per-slot "occupied" bit-packing; O(1) count, O(n) add/remove with shift, O(1) contains via binary search |
| API surface | `Iter_Init`, `Iter_Add`, `Iter_Remove`, `Iter_Contains`, `Iter_Count` — the five the spec's "all API" decision named; `foreach` walks `0..count` after these maintain the compact run. | Full but bounded; `Free`/multi-dim/`Random` deferred (YAGNI) |
| Walk semantics | `foreach` body receives each value once, ascending, no duplicates. | Set semantics, the point of replacing a linked-list iterator with a sorted set |
| Native implementation | `Iter_*` are **native stubs in the compiler's runtime layer** (`amx.c`/host), NOT script functions — so they are fast and uniform, and `foreach`'s codegen assumes they exist. | Matches how `numargs`/`getarg` are exposed; keeps the loop body free of per-iteration function-call overhead |
| `break`/`continue` | Supported, as in any loop. | `foreach` is a real loop statement in the parser |

## 4. Technical Design

### 4.1 Data layout (compiler-maintained)

For `new X[capacity]` used as a set:

```
X[0 .. count-1]  : sorted distinct in-use values
count            : number of in-use items, stored in a parallel
                   symbol the compiler allocates (e.g. an implicit
                   <count> cell next to the array, or passed by the
                   user via Iter_Count)
X[count .. capacity-1] : free
```

The maintainer functions:
- `Iter_Init(X, cap)` → zero the run, count=0
- `Iter_Add(X, v)` → binary-search the run for `v`; if absent, shift the tail right by one and insert (count++)
- `Iter_Remove(X, v)` → binary-search; if present, shift the tail left (count--)
- `Iter_Contains(X, v)` → binary-search, returns bool
- `Iter_Count(X)` → returns count

Complexity: O(log n) contains, O(n) add/remove. For typical SA-MP set
sizes (players, vehicles) n is small; the scan/binary-search is cheap
and the codegen is simple.

### 4.2 `foreach` codegen

In `doforeach()` (new, mirroring `dofor()` at sc1.c:6220):

```
; for (new i : X) { BODY }
; emits, with i as the loop variable (a local in the loop scope):
  LCTRL 5            ; (or load i's address)
  add.c  0           ; i = X[0]
  jzer   @done       ; if count==0, nothing to do
@loop:
  <user body codegen>            ; i is live here
  load.i               ; i = X[i]  (next value)
  add.c  1
  jzer   @done
  jump   @loop
@done:
```

The emitted loop uses **only existing AMX opcodes** (same constraint as
experiment 001), so unmodified hosts run the output. The body runs with
the loop variable `i` bound to each in-use value in ascending order.

Because `foreach` is a loop statement, `addwhile()`/`readwhile()` and
`scanloopvariables()` (sc1.c) handle `break`/`continue` exactly as for
`for` — piggybacked, not reinvented.

### 4.3 Parser changes

- New token `tFOREACH` (keyword `foreach`) in `sc.h`/`sc_tokens` (scvars.c),
  placed to keep enum/string-table alignment.
- `doforeach()` in `sc1.c`, called from the statement dispatch
  (`switch (tok)` near `dofor`'s call site, sc1.c:5776 area).
- Grammar: `foreach ( [ new ] <var> : <array> ) <body>`.
  - `new <var>` declares a loop-scoped local; bare `<var>` uses an existing one.
  - `<array>` must be an array symbol of the user's declaration (error
    if not an array / not sized).

### 4.4 Native registration

`Iter_Init/Add/Remove/Contains/Count` registered as natives in the
compiler's native table (the same table `numargs`/`getarg` live in,
`compiler/source/amx/amxcore.c` + the `.def`), with AMX implementations
that operate on the compact sorted run. A `#include <foreach>`-style
header (or the compiler predefines them) exposes the `native`
declarations to script.

### 4.5 Safety & compatibility

- Opt-in: `foreach` is not valid syntax today → no existing script changes.
- No new opcodes → unmodified host compatibility.
- The `Iter_*` natives are new symbols; scripts that already define a
  symbol with those exact names would collide — documented, and the
  default can be namespaced (`PawnIter_Add`) with a `#define` alias if
  needed (decide during implementation, test first).

## 5. Test Plan

Test types: `runtime` for behavior (pawnruns + `#include <console>`),
`output_check` for rejection diagnostics.

| Test | Behavior |
|---|---|
| `foreach_basic` | add 42, 7 → `foreach` prints 7, 42 (ascending, no dup) |
| `foreach_duplicate` | add 5 twice → body sees 5 once |
| `foreach_remove_during` | remove in body → remaining values iterated correctly |
| `foreach_break` | `break` stops mid-iteration |
| `foreach_continue` | `continue` skips a value |
| `foreach_empty` | no items → body never runs |
| `foreach_reject_notarray` | `foreach (i : 42)` → exact error (new number) |
| `foreach_reject_bare` | `foreach` without `:` or array → exact error |
| `iter_contains` | `Iter_Contains` true/false paths |
| `iter_count` | `Iter_Count` tracks add/remove |

Plus a differential note: a script using a small hand-rolled YSI-style
circular iterator produces the same *value sequence* as native
`foreach` on the same add/remove order (semantics equivalence, not
layout equivalence — the layouts differ by design).

## 6. Out of Scope (YAGNI)

- YSI circular-list layout compatibility (we chose the own compact
  layout; not drop-in for existing YSI iterator code).
- `Iter_Free`, multi-dimensional iterators, `Iter_Random*`,
  user-defined `iterfunc` filter functions (the `Player`/`Bot`
  special iterators and their `Iter_Func@` hooks).
- Iterator-of-arrays, hashing (that is `y_hashmap`/`y_bintree`
  territory).

## 7. Success Criteria

1. All §5 tests pass against our modified `pawncc` + `pawnruns`; the
   full upstream suite shows no regressions (2 known pre-existing
   failures only: `__timestamp`, `gh_353_symbol_suggestions`).
2. `foreach` emits only existing AMX opcodes; runs on an unmodified host.
3. `experiments/002-foreach/RESULT.md` documents what was tried, what
   worked, what broke, what is next.
