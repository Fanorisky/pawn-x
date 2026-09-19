# Experiment 003: Extended Iterator API (setfree/setrandom, Reverse, iterfunc)

**Date:** 2026-09-19
**Spec (phase 3):** docs/superpowers/specs/2026-09-19-iterfunc-generators-design.md
**Builds on:** exp 002 (`set_foreach` + compact-set `set*` natives)
**Suite at close:** 125 PASSED / 2 FAILED (only the known pre-existing
baselines `__timestamp`, `gh_353_symbol_suggestions`; no regressions).

## What was tried

Bringing the remaining useful `y_iterate` capabilities into the compiler /
runtime, done in three phases (each spec-lite → TDD red → implement → review):

### Phase 1 — `setfree` + `setrandom` natives
Two natives added to `compiler/source/amx/itercore.c` (registered in
`iter_Natives`, declared in `compiler/include/foreach.inc`):
- `setfree(const array[])` — smallest non-negative integer NOT in the set
  (id-allocation helper). Values are sorted ascending in `array[1..count]`, so
  the first index whose value differs from its expected `k-1` is the gap; a
  dense `0..count-1` run returns `count`. Tests: `set_free` (empty→0, {0,1,3}→2,
  {0,1,2,3}→4).
- `setrandom(const array[])` — a uniformly chosen member, or `-1` if empty.
  Uses a small self-contained xorshift PRNG (no dependency on host `rand()`).
  Test `set_random` pins the membership invariant (every draw is a member) plus
  empty→-1 and single-element cases.

### Phase 2 — `Reverse(operand)` + safe mid-walk removal
`set_foreach (new i : Reverse(data))` iterates the set **descending**.
`Reverse(` is recognised in the operand position (string-match, like the
array-operand and `___` recognition); the pointer walk runs `p` from
`&array[count]` down to `&array[0]` with `jsleq` + `p -= cell` (vs the ascending
`jsgeq` + `p += cell`). Both directions share the elided-`p`-load fold.
- **Safe removal during iteration** falls out for free: in a reverse walk,
  `setremove` shifts the tail (higher values) left — and those are the
  *already-visited* elements — so removing the current element never skips or
  doubles an unvisited one. `foreach_reverse_saferemove` proves it: removing
  every even during a reverse walk of {1..5} visits 5 4 3 2 1 once each and
  leaves count=3 ({1,3,5}). (Contrast: forward mid-walk removal is the
  documented footgun from exp 002 — reverse is the supported safe pattern.)

### Phase 3 — `iterfunc` lazy generators
A function declared `iterfunc Name(cur, ...args)` is a generator: called once
per iteration with the running state `cur`, it returns the next value or
`ITER_STOP` (`= cellmin`) to end; the first call receives `cur == ITER_STOP`
(the seed). `set_foreach (new i : Name(args))` over such a call emits a
**call-loop** instead of an array walk — no backing array, values computed on
the fly (Range/step/filter/… ).
- `tITERFUNC` keyword + `uITERFUNC` usage flag (`0x800`, a free bit); `iterfunc`
  combines with `stock`/`static`/`public`. `const ITER_STOP = cellmin;` in the
  header. Extra args are evaluated **once** at loop entry and cached (proven by
  `iterfunc_step`: a side-effecting arg runs exactly once across a 5-iteration
  loop). `Reverse(` over a generator is rejected (error 255). The call-loop
  uses only existing opcodes (`push`/`call`/`retn`/`const.alt`/`jeq`).
- Tests: `iterfunc_range` (0..4), `iterfunc_empty` (seed→STOP, body never runs),
  `iterfunc_break`, `iterfunc_step` (0 2 4 6 8 + arg-cached-once), `iterfunc_filter`
  (`iterfunc stock` predicate → 0 3 6 9), `iterfunc_reject_reverse`.

## What worked
- All three phases green, full suite 125/2 (no regressions). Every feature is
  opt-in and emits **no new AMX opcode** — output runs on an unmodified
  open.mp server.
- The generator call-loop resolves a **forward-defined** generator correctly
  (required `markusage(gensym, uREAD)`, not a bare `usage|=uREAD`, so the
  symbol is emitted in the write pass — otherwise the `call` resolved to the
  wrong address).

## What broke / subtleties
- `iterfunc` + specifier initially errored 020 (`iterfunc stock` unparsed);
  fixed to route `stock`/`static`/`public` through the normal class-specifier
  path while still setting `uITERFUNC`.
- `iterfunc` is now a reserved word (breaks any pre-existing identifier named
  `iterfunc` — opt-in cost, like `set_foreach`).
- A non-iterfunc, non-array `set_foreach` operand emits a stray `warning 202`
  before the correct `error 255` (noisy diagnostics; dispatch is correct).

## What is next
- `yield`/coroutine-style generators (needs frame suspension — out of scope §6).
- Multi-dimensional / by-reference generator state.
- Fold the stray `warning 202` on a bad operand into just `error 255`.
