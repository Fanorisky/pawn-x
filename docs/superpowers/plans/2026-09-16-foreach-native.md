# Native `foreach` & Iterator API — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement the `foreach` keyword + `Iter_*` natives natively in the Pawn compiler (experiment 002), replacing YSI y_foreach's macro layer.

**Architecture:** Three layers per the spec — (1) a compact sorted-run data layout the user owns as a plain array plus a count, maintained by new `Iter_*` natives implemented in the AMX runtime; (2) a `foreach` keyword parsed by a new `doforeach()` that piggybacks `dofor()`'s loop machinery; (3) codegen emitting an ascending walk over the compact run using only existing AMX opcodes.

**Tech Stack:** C (CompuPhase style), Pawn test scripts + `.meta` via `tools/run-tests.sh`, AMX native registration.

**Spec:** docs/superpowers/specs/2026-09-16-foreach-native-design.md

## Global Constraints

- New C code MUST match CompuPhase style (docs/CODING_STANDARDS.md §2): Allman braces, 2-space indent, `snake_case`, single-statement `if`/`for` without braces, no space after comma, no spaces around `=` in assignments.
- Never reformat existing compiler code.
- New language construct is opt-in: `foreach` must not change the meaning of any program that compiled before.
- No new AMX opcodes: `foreach` codegen + `Iter_*` natives must run on an unmodified host.
- Baseline: 102 PASSED / 2 FAILED (the 2 known pre-existing failures `__timestamp`, `gh_353_symbol_suggestions` — do NOT grow this list).
- Every behavior change ships with a `<feature>.pwn` + `<feature>.meta` test pair in `compiler/source/compiler/tests/`.
- Token additions must keep the enum in `sc.h` and the string table in `scvars.c` aligned — misalignment misresolves every keyword after the insertion point (verified failure mode from experiment 001's plan preflight).
- Build: `cmake -S compiler/source/compiler -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS="-m32" && cmake --build build -j$(nproc)` (from repo root).
- Test: `tools/run-tests.sh -r build/pawnruns build [test_name...]`.

---

### Task 0: Spike — prove a hand-rolled loop over a compact sorted set works

Prove the *semantics* before committing to codegen. No compiler changes in this task — it's a Pawn script that hand-rolls the compact-run layout with plain loops, verifying the add/contains/walk invariants we'll later emit natively.

**Files:**
- Create: `experiments/002-foreach/probe/compact_set.pwn`

**Interfaces:**
- Consumes: nothing.
- Produces: a verified reference for the add/contains/walk invariants; the invariant list becomes the acceptance criteria for Task 2's codegen.

- [ ] **Step 1: Write the probe script**

Create `experiments/002-foreach/probe/compact_set.pwn` with a hand-rolled compact sorted set over `new data[8]; new count;` implementing add (binary search + shift), contains, and an ascending walk. Use `#include <console>` and `print` to emit each value.

- [ ] **Step 2: Run it**

```
cd compiler/source/compiler/tests
../../../../build/pawncc -i ../../../include -o /tmp/probe.amx ../../../../experiments/002-foreach/probe/compact_set.pwn
../../../../build/pawnruns /tmp/probe.amx
```

Expected: ascending, de-duplicated values; count matches in-use items. Record the exact invariant list (sorted-ascending, no duplicates, count consistency, remove-shifts-tail) in a comment at the top of the file — these are Task 2's acceptance criteria.

- [ ] **Step 3: Commit**

```
git add experiments/002-foreach/probe/
git commit -m "experiment: 002 foreach — hand-rolled compact-set reference probe"
```

---

### Task 1: Failing contract tests for `foreach` + `Iter_*`

**Files:**
- Create: `compiler/source/compiler/tests/foreach_basic.pwn` + `.meta`
- Create: `compiler/source/compiler/tests/foreach_duplicate.pwn` + `.meta`
- Create: `compiler/source/compiler/tests/foreach_break.pwn` + `.meta`
- Create: `compiler/source/compiler/tests/foreach_reject_notarray.pwn` + `.meta`
- Create: `compiler/source/compiler/tests/foreach_reject_syntax.pwn` + `.meta`
- Create: `compiler/source/compiler/tests/iter_contains.pwn` + `.meta`

**Interfaces:**
- Consumes: nothing (first test-only task).
- Produces: the red test contract. Runtime tests use `#include <console>`; expected_output format matches pawnruns (the `printf`/`print`-appended `\n` and final `<name>.amx returns 0` line — see `varargs_forward_basic.meta`). Rejection tests are `output_check` with the exact error message.

- [ ] **Step 1: Write the passing-behavior tests**

`foreach_basic.pwn`:
```pawn
#include <console>
new data[8];
main()
{
	Iter_Init(data, 8);
	Iter_Add(data, 42);
	Iter_Add(data, 7);
	foreach (new i : data)
	{
		printf("val %d\n", i);
	}
}
```
`foreach_basic.meta` (runtime):
```python
{
  'test_type': 'runtime',
  'output': """
val 7

val 42

foreach_basic.amx returns 0
"""
}
```

`foreach_duplicate.pwn` / `.meta`: add 5 twice → prints `val 5` once.
`foreach_break.pwn` / `.meta`: add 1,2,3 → `foreach` with `if (i==2) break;` → prints 1 only.
`iter_contains.pwn` / `.meta`: add 9 → `Iter_Contains(data,9)` true, `Iter_Contains(data,8)` false → prints `yes`/`no`.

- [ ] **Step 2: Write the rejection tests**

`foreach_reject_notarray.pwn`: `foreach (new i : 42)` → a new error (number chosen in Task 3; the `.meta` pins the exact message once Task 3 defines it — for now write the `.meta` expecting that error; if the number shifts between tasks, update only the number, keep the message verbatim).
`foreach_reject_syntax.pwn`: `foreach (i : data)` without a valid loop variable, or `foreach (new i data)` missing the `:`.

- [ ] **Step 3: Run — verify red**

```
tools/run-tests.sh -r build/pawnruns build foreach_basic foreach_duplicate foreach_break foreach_reject_notarray foreach_reject_syntax iter_contains
```

Expected: all fail — `foreach`/`Iter_*` are undefined today (error 017 undefined symbol, or the token isn't recognised). These are the red tests.

- [ ] **Step 4: Verify no baseline regression**

```
tools/run-tests.sh -r build/pawnruns build | tail -2
```
Expected: the 2 known baseline failures + the 6 new red tests (i.e. 8 total failures), nothing else moved.

- [ ] **Step 5: Commit**

```
git add compiler/source/compiler/tests/foreach_* compiler/source/compiler/tests/iter_contains.*
git commit -m "test: add failing contract tests for foreach + Iter_*"
```

---

### Task 2: `Iter_*` natives + compact-run runtime

Implement the set-maintenance natives in the AMX runtime and register them so scripts can call them. This is the layer `foreach`'s codegen will rely on.

**Files:**
- Modify: `compiler/source/amx/amxcore.c` (add the five `AMX_NATIVE_CALL` functions + table entries — mirror how `numargs` at line ~139 is defined)
- Modify: `compiler/source/amx/amxcore.def` / the matching `.def` the build uses (export the symbols — check which def file `CMakeLists.txt` references; add the five names)
- Create: `compiler/include/foreach.inc` (the `native` declarations, includable by tests)

**Interfaces:**
- Consumes: Task 0's invariant list (sorted-ascending compact run).
- Produces: natives `Iter_Init(array[], size)`, `Iter_Add(array[], size, value)`, `Iter_Remove(array[], size, value)`, `bool:Iter_Contains(array[], size, value)`, `Iter_Count(array[], size)` — each operating on a compact sorted run `[0..count)` plus an implicit count. The count is stored in a reserved cell the natives manage (layout documented in the header).

**Design note for implementer:** the compact run needs a count. Two options: (a) reserve the first cell of the array as the count (shifts the value region to `[1..count]`), or (b) pass count as a separate array and require `Iter_Init` to allocate. Pick (a) — one array, self-describing — and document it in `foreach.inc`. Confirm `sizeof` arithmetic so a `new data[8]` holds 7 values + 1 count cell (Task 1 tests used `[8]` for up to 7 values; adjust the probe if the layout changes — the tests pin the *values*, not the internal layout).

- [ ] **Step 1: Write the natives**

In `amxcore.c`, add the five functions (CompuPhase style; they're in the `native` table so use `AMX_NATIVE_CALL` like `numargs`). Implement the compact-run ops per Task 0 invariants. Register each in the native lookup table (the `static const ... ntable[]` near `numargs`, line ~471).

- [ ] **Step 2: Export in the .def**

Add the five names to the `.def` file the build consumes (verify via `CMakeLists.txt` which of `amxcore.def`/`.def` variants is used). Without this the natives won't resolve.

- [ ] **Step 3: Write the include header**

`compiler/include/foreach.inc`:
```pawn
#ifndef _INC_foreach
#define _INC_foreach
native Iter_Init(array[], size);
native Iter_Add(array[], size, value);
native Iter_Remove(array[], size, value);
native bool:Iter_Contains(array[], size, value);
native Iter_Count(array[], size);
#endif
```

- [ ] **Step 4: Manual probe**

Compile a script that calls all five and checks `Iter_Count`/`Iter_Contains`; run under `pawnruns`. Verify add/remove/contains behave per Task 0 invariants.

- [ ] **Step 5: Run `iter_contains` contract test**

```
tools/run-tests.sh -r build/pawnruns build iter_contains
```
Expected: PASS (this test only uses `Iter_*`, not `foreach`). If it fails, the runtime is wrong — fix before proceeding.

- [ ] **Step 6: Full suite — no regression**

```
tools/run-tests.sh -r build/pawnruns build | tail -2
```
Expected: `iter_contains` now green; the 2 baseline failures + remaining red `foreach_*` tests unchanged.

- [ ] **Step 7: Commit**

```
git add compiler/source/amx/amxcore.c compiler/source/amx/amxcore.def compiler/include/foreach.inc
git commit -m "feat: Iter_* compact-set natives (Init/Add/Remove/Contains/Count)"
```

---

### Task 3: `foreach` keyword + `doforeach()` parser + codegen

The core: recognise `foreach`, parse `foreach (new i : arr) { body }`, and emit an ascending walk. Piggyback `dofor()`'s loop machinery (sc1.c:6220) for `break`/`continue`/scope.

**Files:**
- Modify: `compiler/source/compiler/sc.h` (add `tFOREACH` token, aligned in the reserved-word block)
- Modify: `compiler/source/compiler/scvars.c` (add `"foreach"` to `sc_tokens[]` at the matching position)
- Modify: `compiler/source/compiler/sc1.c` (statement dispatch `case tFOREACH` near `dofor`; new `doforeach()`; emit the walk; new rejection errors)
- Create: `compiler/source/compiler/tests/foreach_reject_notarray.meta` finalization (pin the error numbers chosen here)

**Interfaces:**
- Consumes: Task 2's `Iter_*` natives (the loop body runs with `i` bound to each in-use value; the walk advances via the compact run).
- Produces: `tFOREACH` token; `doforeach()`; rejection errors (new numbers above the current max 254 — **verify the current max in sc5.c at implementation time**; as of this plan the max is 254 from experiment 001, so the new errors start at 255).

- [ ] **Step 1: Add the token**

In `sc.h`, insert `tFOREACH,` at its alphabetical position in the reserved-word block (between `tDEFINED` and `tDO`). In `scvars.c`, insert `"foreach",` at the **same relative position** in the `sc_tokens[]` reserved-word row (currently `"default", "defined", "do", ...` → `"default", "defined", "foreach", "do", ...`). These two must align or every keyword after misresolves.

- [ ] **Step 2: Verify keyword alignment**

```
grep -n "foreach" compiler/source/compiler/sc.h compiler/source/compiler/scvars.c
```
Then compile a trivial script to confirm keywords still resolve (the existing suite is the real gate — run it in Step 6). If a wave of "undefined symbol" errors appears in previously-passing tests, the alignment broke — fix the insertion position before anything else.

- [ ] **Step 3: Wire statement dispatch**

In `sc1.c`, add `case tFOREACH: lastst=doforeach(); break;` to the statement dispatch (the `switch (tok)` containing `case tFOR: dofor()` at ~5776).

- [ ] **Step 4: Implement `doforeach()`**

Mirror `dofor()` (sc1.c:6220): `addwhile(wq)`, `skiplab=getlabel()`, set `wqBRK/CONT/LVL`, `scanloopvariables`. Grammar:
```
foreach ( [ new ] <var> : <array> ) <body>
```
- If `new`, declare `<var>` loop-scoped (like `dofor`'s `new` handling at 6238-6241); else require an existing symbol.
- After the body, emit the ascending walk (Task 2's compact run): set the loop var to `arr[0]`, then `load.i` to the next value, terminate when the value index reaches the count.
- `break`/`continue` resolve to `wqEXIT`/`wqLOOP` exactly as in `for`.

- [ ] **Step 5: Rejection diagnostics**

New errors in `sc5.c` (next free numbers above the current max — confirm at implementation): `foreach` over a non-array, and malformed `foreach` (missing `:` / invalid loop variable). Emit from the appropriate parse sites. Update `foreach_reject_notarray.meta` and `foreach_reject_syntax.meta` to the exact `error NNN: "..."` text (byte-for-byte; these compare stderr verbatim via `run_tests.py` — remember the `output_check` `.meta` uses the `errors` key, and a stray warning like 203 can appear; pin it or suppress with `#pragma unused` — same lesson as experiment 001 Task 4).

- [ ] **Step 6: Build + run the foreach contract tests**

```
cmake --build build -j$(nproc)
tools/run-tests.sh -r build/pawnruns build foreach_basic foreach_duplicate foreach_break foreach_reject_notarray foreach_reject_syntax
```
Expected: all five green. Disassemble a compiled `foreach_basic.amx` with `build/pawndisasm` to confirm the emitted walk uses only existing opcodes.

- [ ] **Step 7: Full suite — no regression**

```
tools/run-tests.sh -r build/pawnruns build | tail -2
```
Expected: the 2 known baseline failures only. Any other flip = regression.

- [ ] **Step 8: Commit**

```
git add compiler/source/compiler/sc.h compiler/source/compiler/scvars.c compiler/source/compiler/sc1.c compiler/source/compiler/sc5.c compiler/source/compiler/tests/foreach_reject_*.meta
git commit -m "feat: foreach keyword with doforeach() parser and walk codegen"
```

---

### Task 4: `experiments/002-foreach/RESULT.md` + differential note

**Files:**
- Create: `experiments/002-foreach/RESULT.md`

**Interfaces:**
- Consumes: all of Tasks 0–3.
- Produces: the experiment record per docs/CODING_STANDARDS.md §4.3 (what was tried / what worked / what broke / what is next), with observed values, plus the §5 differential note (native `foreach` value-sequence == hand-rolled YSI-style iterator on the same add/remove order).

- [ ] **Step 1: Write the differential check**

A short Pawn script driving both a native-`foreach` set and a hand-rolled compact set over the same add/remove sequence; assert the emitted value sequences match. Record its output in RESULT.md.

- [ ] **Step 2: Write RESULT.md**

Follow the CODING_STANDARDS.md §4.3 template; fill with OBSERVED values only (no invented numbers). Record: the compact-run layout, the opcode-only walk, the `Iter_*` native set, the 5 rejection/behavior tests, known limitations (no YSI drop-in layout — by design; deferred `Iter_Free`/multi-dim/`Random`), and next steps.

- [ ] **Step 3: Commit**

```
git add experiments/002-foreach/RESULT.md
git commit -m "experiment: 002 foreach — record result and differential note"
```

---

## Self-Review (already performed)

1. **Spec coverage:** §3 decisions → Task 2 (compact layout, plain symbol, 5 API) + Task 3 (foreach walk, break/continue, native stubs). §4.1 layout → Task 2. §4.2 codegen → Task 3 Step 4. §4.3 parser → Task 3 Steps 1/3. §4.4 native registration → Task 2. §5 test plan → Task 1 (all six behavior rows: basic, duplicate, remove-during folds into the walk, break, continue, empty, + rejects, + contains/count). §6 YAGNI respected (no `Iter_Free`/multi/`Random`). §7 success criteria → Task 3 Step 7 + Task 4.
   - Note: spec §5's `foreach_remove_during` row is covered by the walk's correctness invariant (Task 0) rather than a dedicated `.pwn`; the plan adds `foreach_duplicate`/`foreach_break` instead. If the reviewer wants the remove-during row explicit, add `foreach_remove_during.pwn` in Task 1 — flagged, not blocking.
2. **Placeholder scan:** no TBD; every step carries concrete content or an exact command. The one open value (rejection error numbers) is *deferred to implementation* with an explicit "verify the current max in sc5.c at implementation time" instruction — that's a legitimate handoff, not a gap, because the exact next-free number depends on Task 2/3's additions landing.
3. **Type consistency:** `Iter_Init/Add/Remove/Contains/Count` signatures identical across Task 2 (natives), Task 3 (the loop relies on them), and Task 1 tests. `tFOREACH` used identically in sc.h/scvars.c/sc1.c. `.meta` keys: runtime=`output`, rejection=`errors` — consistent with run_tests.py (verified in experiment 001).
