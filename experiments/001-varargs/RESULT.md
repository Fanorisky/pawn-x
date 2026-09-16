# Experiment 001: Native Varargs Forwarding (`___`)

**Date:** 2026-09-15 (implementation), 2026-09-16 (final verification)
**Compiler commit at run:** 57c3a7daa0f678a574fef2f13fb71d50d1cc9dea
**Spec:** docs/superpowers/specs/2026-09-15-varargs-forwarding-design.md

## What was tried

Implementing y_va's `___` forwarding as native compiler codegen: a token
recognized in argument position, a copy sequence in callfunction(), and a
dynamic byte count for the callee. Replaces YSI's runtime bytecode scanning
and rewriting (y_va/impl.inc YVA2_Initalise/YVA2_DoPush).

The implemented semantics (deviation from the spec's §4.1 formula, which
contradicted the spec's own §2 examples and was ruled a spec defect):
bare `___` forwards exactly the variable arguments (skips the named
parameters); `___(N)` forwards from absolute argument index N (0-based),
mirroring y_va's `va_start<N>`. Recognition is a string match in the
argument loop (no keyword-table insertion — `___` remains a valid
identifier, so `new ___ = 5` keeps its old meaning).

## What worked

- `varargs_token_smoke` — `___` recognized in argument position (runtime).
- `varargs_forward_basic` — bare `___` passthrough into printf (runtime).
- `varargs_forward_skip` — `___(2)` skip arithmetic (runtime, should_fail:
  first line pins the skip, then amxcons printf aborts on arg-count
  mismatch; see "What broke").
- `varargs_forward_compat` — `___` as an ordinary identifier in
  non-variadic code compiles and runs unchanged (runtime).
- `varargs_forward_reject` — `___` in a function without `...` is
  error 253 (output_check).
- `varargs_forward_nested` — an outer `___` call inside the argument list
  of a call whose arguments themselves contain `___` (y_va2_Nesting
  shape, runtime).
- `varargs_forward_dynamic` — `___` forwarded into
  `CallLocalFunction("Target", "ii", ___)` dispatching to a public
  (compile-time output_check; see "What broke" for the runtime reduction).
- Recursion, chaining, and nesting probes passed (Task 3 contract
  verification); -O0, -O1 and default builds produce identical code.
- The copy loop is a self-contained downward-walking opcode sequence —
  pure Option A codegen, no memcpy-native fallback needed (Option B was
  the locked fallback, never taken). Dynamic byte count for the callee
  with negative clamp; native-call cleanup preserves the return value via
  a heap temp.
- Final suite: 98 PASSED, 2 FAILED — the only failures are the known
  baseline `__timestamp` and `gh_353_symbol_suggestions`.

## What broke

- The spec's §4.1 formula (`src_off=(skip+named+3)*cell`) was defective;
  the implemented semantics above is what the contract tests pin
  empirically. Spec amendment queued.
- amxcons's printf raises `AMX_ERR_NATIVE` when argument count does not
  match the format specifiers — in both directions. The skip test's
  intended "pinned 0" for a missing 4th argument is unachievable on this
  runtime; the meta records the observed abort (`should_fail: True`).
- The brief's compat test used `new arr[2] = {7, ___}`, which is invalid
  Pawn on an unmodified compiler (error 008) — test bug, minimally
  rewritten preserving intent and output.
- Error 253 lands in the >=200 warning-numbering range, so a naive
  insertion would have printed "warning 253" and not failed the build.
  Fixed by reclassifying numbers 253+ as errors in sc5.c and both
  pc_error hosts. Policy side effect recorded: 253–299 is now
  "errors in warning numbering"; future warnings must number below 253.
- `CallLocalFunction` is an open.mp/SA-MP host native absent from both
  the vendored compiler/include and pawnruns (which registers only
  console/string/core natives). Proven, not assumed: the brief's verbatim
  dynamic .pwn fails to compile with `error 017: undefined symbol
  "CallLocalFunction"`, and with the native declared the compiled .amx
  aborts under pawnruns with `Run time error 19: "File or function is
  not found"`. Pawn has no script-level function pointers, so no
  script-level equivalent exists; the test was reduced per the plan to a
  compile-time output_check capturing the exact dispatch pattern, with
  the native declared in the .pwn. **Runtime dispatch-passthrough
  validation requires the open.mp server environment — a manual step
  outside this suite.**

Known limitations (accepted, documented): silent under-forwarding when
the target has fixed arity; forwarded variadic arguments are references
(a caller's value changes are visible); array-returning targets untested.

## What is next

- Spec amendment (queued): §4.1 formula (implemented semantics above);
  §3 compat gate refinement (a resolvable `___` symbol in a non-variadic
  function keeps its old meaning; in variadic functions the token wins
  over same-named symbols); note that third-party pc_error hosts print
  "warning 253"; the 253–299 errors-in-warning-numbering policy.
- Manual validation of runtime dispatch passthrough under open.mp
  (CallLocalFunction + `___` into a public).
- Skipped/parked items from the spec's YAGNI list (mid-list `___`
  position rule, etc.).
- Benchmark Option A (self-contained downward-walking loop, implemented)
  vs Option B (memcpy-native) codegen — A worked first, so B was never
  benchmarked.
- Remaining deferred review nits: error 253 is not counted toward the
  error-107 three-per-line guard (sc5.c, `number<200 || errwarn`);
  pc_enablewarning(253) NITs.
- Upstream PR readiness: split the feature for the CompuPhase/pawn
  upstream (open.mp's compiler fork is the realistic first target).
