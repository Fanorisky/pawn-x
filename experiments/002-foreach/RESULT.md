# Experiment 002: Native `foreach` & Iterator API

**Date:** 2026-09-18
**Compiler commit at run:** 14eea6a941f9f5c01b5413d82c77b780b1f9bfac
**Spec:** docs/superpowers/specs/2026-09-16-foreach-native-design.md
**Build:** `cmake -S compiler/source/compiler -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS="-m32" && cmake --build build -j$(nproc)`

## What was tried

Moving YSI `y_foreach` / `y_iterate` from a macro-and-linked-list script
layer into the compiler: a real `foreach` loop keyword with codegen, a
compiler-managed compact-set data layout, and native `Iter_*` functions
that maintain that layout. Replaces YSI's circular sorted linked-list
(`F@`/`Y_FOREACH_` obfuscated macros) and its `setadd/Remove/Contains`
stock functions.

Four pieces were built across the feature commits (`ee8a84e` probe,
`369fa53`/`6137c13` red tests, `1fa40d8`/`7a8dc1e` natives, `14eea6a`
keyword+codegen):

1. **Compact sorted-run layout.** A plain script array is used as a set:
   `array[0]` holds the count, `array[1..count]` holds the distinct
   in-use values in ascending order; `array[count+1..]` is free. No tag
   machinery, no per-slot occupied bits — a value is present iff it lies
   in the sorted run.

2. **Five natives** in `compiler/source/amx/itercore.c`:
   `setinit/Add/Remove/Contains/Count`. All are **reference-based** —
   they take the array by reference (`const array[]`), not by name,
   because public arrays are forbidden (error 056), so a name-based API
   was infeasible. `setadd` binary-inserts at the sorted position and
   shifts the tail; `setremove` shifts the tail left; `sethas`
   scans the run; `setlen` returns `array[0]`.

3. **`foreach` keyword** → `doforeach()` in `sc1.c` (declared line 135,
   dispatched at `tFOREACH` line 5779, defined line 6339). It emits an
   indexed walk `k = 1..count` binding the loop variable to `array[k]`,
   reusing `dofor`'s loop machinery for `break`/`continue`. The count is
   snapshotted once at loop entry.

4. **Error 255** for `foreach` over a non-array/value and for malformed
   `foreach` syntax (missing `: <array>`), classified as an error via
   experiment 001's ">=253 numbers are errors" policy.

The `<foreach>` header (`compiler/include/foreach.inc`) exposes the five
`native` declarations.

## What worked

- **Full upstream suite: 110 PASSED, 2 FAILED.** The only failures are
  the two known pre-existing baselines — `__timestamp` and
  `gh_353_symbol_suggestions` — unrelated to this feature.

- **All 8 foreach/iter behavior + rejection tests pass** (in
  `compiler/source/compiler/tests/`):
  - `foreach_basic` — add 42, 7 → walk prints 7 then 42 (ascending).
  - `foreach_duplicate` — add 5 twice → body sees 5 once, count=1.
  - `foreach_remove_during` — remove-in-body + break behaves per the
    snapshot semantics (count drops to 2).
  - `foreach_break` — `break` stops mid-iteration.
  - `foreach_empty` — empty set → body never runs.
  - `foreach_reject_notarray` — `foreach (new i : 42)` → exact
    `error 255: "foreach" iterates over an array or iterator, not a value`.
  - `foreach_reject_syntax` — `foreach (new i data)` → exact
    `error 255: "foreach" syntax requires ": <array>" after the loop variable`.
  - `iter_contains` — true/false paths + count tracking.

- **Opcode-only walk (no new opcode).** Disassembly of the compiled
  differential (`pawndisasm`) shows the walk uses only existing AMX
  mnemonics — `load.s.pri`, `load.i`, `lidx`, `idxaddr`, `inc`,
  `jsgeq`/`jsleq`/`jsless`/`jzer`/`jump`, `sysreq.c` for the natives, etc.
  No `foreach`- or `iter`-specific opcode appears; `Iter_*` are ordinary
  `sysreq.c` native calls. Unmodified hosts run the output.

- **Differential check** (`experiments/002-foreach/differential.pwn`)
  drives the native `foreach`/`Iter_*` set and a hand-rolled compact set
  (probe logic) over the SAME sequence — add 20, 7, 30, 7 (dup), 3;
  remove 20; add 15; remove 999 (absent) — and compares the emitted value
  sequences. Observed output under `pawnruns`:

  ```
  native  n=4: 3 7 15 30
  handroll n=4: 3 7 15 30
  MATCH=1
  native count=4 handroll count=4
  ```

  Both sets emit the identical ascending, de-duplicated sequence
  `3 7 15 30` (7 de-duplicated, 20 removed, 999 no-op), and both report
  count 4 — semantic equivalence confirmed. (`console` printf emits a
  trailing newline per call, so the raw run shows one value per line;
  the values and order are as above.)

## What broke

- **Name-based API was infeasible.** The spec sketched `setadd(MySet, v)`
  passing the set by name; public arrays are forbidden (error 056), so the
  API was reworked to reference-based (`const array[]`). Contract tests
  were updated accordingly (`6137c13`).

- **`setadd` initially overwrote slot 1** instead of inserting at the
  sorted position, breaking the ascending/distinct invariant; fixed in
  `7a8dc1e` (binary-insert + tail shift), which also reconciled the
  capacity docs.

- **Layout differs from the spec's first sketch.** §4.1 floated
  `array[0..count-1]` values with a separate parallel count cell; the
  shipped layout stores the count in `array[0]` and values in
  `array[1..count]`. This is intentional (self-describing array, no
  parallel symbol) and is what the differential's hand-rolled reference
  is reconciled against at the value-sequence level.

Known limitations (accepted, documented honestly):

- **No capacity bound-check in `setadd`** (V1): arrays are passed by
  reference without a size, so an add past capacity is unguarded. A
  size-carrying API or a compiler-tracked capacity is the fix.
- **Only the global-array `foreach` path is suite-tested.** The
  function-local `iARRAY` and by-reference `iREFARRAY` walk paths are
  codegen-correct but currently unexercised by a `.pwn` test.
- **Count is snapshotted at loop entry** (standard `foreach` semantics):
  adds/removes during the loop are not re-read. `foreach_remove_during`
  pins the remove+break behavior; `foreach_remove_nobreak` pins the
  ACTUAL remove-without-break behavior — a mid-walk `setremove` shifts
  the tail left, so one value is skipped and another emitted twice.
  Mutating the set mid-walk without breaking is unsupported and now
  documented by that test.
- **Not a YSI drop-in.** The compact sorted-run layout is deliberately
  not YSI's circular linked-list, so existing YSI iterator code is not
  binary-compatible — semantic replacement, not layout replacement.
- **Deferred (YAGNI, spec §6):** `Iter_Free`, multi-dimensional
  iterators, `Iter_Random*`, user-defined `iterfunc` filter functions.

## Extension: multi-set via any array-reference operand (2026-09-18)

`foreach` now accepts ANY expression that resolves to an array reference,
not just a bare array symbol. This gives **multi-set iteration for free**
over 2D-array rows — no new natives, no YSI-style `Iterator:name<slots,size>`
machinery:

```pawn
new sets[3][8];              // 3 independent compact sets, one contiguous alloc
setadd(sets[0], 42);
setadd(sets[2], 3);
foreach (new i : sets[0]) { ... }   // iterate row 0 only
new k = 2;
foreach (new j : sets[k]) { ... }   // computed index, evaluated once at entry
```

- The `Iter_*` natives already operate on a row `sets[k]` (passed by
  reference) — verified before any compiler change. Only the `foreach`
  parser+codegen needed extending.
- The operand address is evaluated **once** before the loop and cached in
  a hidden loop-scoped cell; the count read and each value read reload from
  that cell (no per-iteration re-evaluation of a computed index).
- A scalar operand (e.g. `sets[0][1]`, a single cell) is rejected with
  error 255 (`foreach_reject_scalar`).
- Heap-allocating operands work: `foreach (i : GetSet())` where `GetSet`
  returns an array — the operand's heap temporary is freed at loop exit
  (also on `break`), verified net-zero over 100 passes
  (`foreach_heap_operand`).
- Cheaper and broader than YSI's multi-dimensional iterators: real Pawn 2D
  arrays, standard indexing, arbitrary slot count/size per declaration,
  no macro layer, no runtime bookkeeping.

**Known limitation (deferred):** an early `return` directly out of a
`foreach` body whose operand allocated heap skips the loop-exit `modheap`
and leaks that temporary on that path only (OP_RETN does not reset HEA).
Strictly better than pre-fix (which leaked on all paths); the correct fix
hooks the function's return-path heap cleanup — a broader change deferred
as a follow-up.

## What is next

- **Early-return heap cleanup** for heap-operand `foreach` (the deferred
  limitation above).
- **Capacity bound-check** in `setadd` (size-aware API or
  compiler-tracked capacity) to close the V1 unguarded-add gap.
- **Tests for the local `iARRAY` and by-ref `iREFARRAY` foreach paths**
  (the multi-set extension now exercises subscripted `iREFARRAY` rows,
  partially closing this).
- **`continue` is now tested** (`foreach_continue` — over {1,2,3},
  `continue` on 2 prints 1 then 3), and **remove-without-break behavior is
  now pinned** by `foreach_remove_nobreak`, closing the two spec §5
  coverage gaps a fresh review flagged.
- **Upstream PR readiness:** split the keyword+codegen and the natives
  for the open.mp compiler fork; the reference-based API and the
  no-new-opcode walk keep the host-side surface minimal.
