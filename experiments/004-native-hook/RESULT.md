# Experiment 004: Native `hook` — Single-Unit Multi-Hook

**Date:** 2026-09-22
**Spec:** docs/superpowers/specs/2026-09-19-native-hook-design.md
**Replaces:** YSI `y_hooks` (the part a compiler can own)
**Suite at close:** 136 PASSED / 2 FAILED (only the known pre-existing
baselines `__timestamp`, `gh_353_symbol_suggestions`).

## What was tried

Letting a callback have MANY handlers in one script:

```pawn
hook OnFoo(a) { printf("A %d", a); return HOOK_CONTINUE; }
hook OnFoo(a) { printf("B %d", a); return HOOK_CONTINUE; }
// calling OnFoo(7) runs both, in source order
```

Return-control constants (`compiler/include/hook.inc`): `HOOK_CONTINUE`=1
(run next, chain result 1), `HOOK_CONTINUE_0`=0 (run next, result 0),
`HOOK_STOP`=-1 (halt, callback returns 0), `HOOK_STOP_1`=-2 (halt, returns 1).

## Mechanism — pure codegen, no bytecode scan, no new opcodes

1. Each `hook Name(args) body` parses as an ordinary function under a hidden
   name `_hook.Name.<seq>` (seq = source order). The `hook` keyword (token
   `tHOOK`) sets a parse flag; the hidden rename sidesteps the duplicate-symbol
   gate (error 021).
2. A per-pass registry records, per callback, the ordered hidden symbols + the
   first hook's arg signature.
3. `hook_emit_dispatchers()` runs right after `parse()` in BOTH compiler passes
   and synthesises one `public Name(args)` dispatcher, driving codegen helpers
   directly (`push.s`/`pushval`/`ffcall`/`move.alt`/`eq.c.alt`/`jnz`/`ffret`):
   calls each hidden hook in order, applies return-control, returns the
   accumulated result. Disassembly confirms `OnFoo` is a plain public calling
   the hidden procs with `call`/`push`/branch — no new opcode.

## What worked

- All four return-control values, source-order dispatch, direct `Name(args)`
  invocation, and single-hook (behaves like a normal callback) — green.
- **The hard part (deferred two-pass synthesis) held.** Address consistency
  between statFIRST and statWRITE was the risk (drift → calls resolve wrong →
  crash/recursion). Solved by: registry reset in `resetglobals()` so each pass
  re-registers the same set in the same order; hidden hooks marked `uREAD`
  before their bodies parse (else the statWRITE dead-code skip drops them); the
  dispatcher `uDEFINE`'d in statFIRST (else uMISSING → error 4); calls resolve
  by name at assembly time to the final address. Verified by disasm + a
  multi-group/interposed-function stress test — no drift.
- **Signature shape-check:** hooks of one callback must have structurally
  identical arg lists — class (value/ref/array/varargs) + array dims per
  position, names ignored (forwarded by slot). Mismatch → error 256.
- Reference and array args forward correctly through the chain (a `&x` written
  by hook A is seen by hook B and by the caller; arrays forward by address).

## What broke / fixed in review

- **Signature check was count-only** (first cut): `hook OnY(a)` + `hook OnY(a[])`
  compiled clean and the array hook read garbage (value forwarded where an
  address was expected). Fixed to compare shape per position; pinned by
  `hook_reject_shape`.
- Hidden-name seq buffer reserved only 2 digits (latent 1-byte overflow at ≥100
  hooks of a max-length name); widened + length-checked.
- Hidden names use `_hook.` not `@hook.` (`@` is PUBLIC_CHAR and would wrongly
  export the hidden functions) — mechanical, no semantic change.

## Boundary vs YSI (honest)

- **Cross-`.amx` is NOT this feature and does not need to be:** open.mp already
  dispatches every callback to the main script AND all side scripts
  (`CallInSides`/`CallAllInSidesFirst`, Manager.hpp), so a hook in another
  filterscript already fires. The gap was only "many bodies in ONE `.amx`",
  which is compile-time — this feature closes it fully.
- **Better than y_hooks** for the static-handler case: no runtime bytecode
  scan/rewrite (YSI's `Hooks_GetStubEntry` disasm + `y_amx` rewrite +
  ALS states), deterministic (not optimizer-pattern dependent, JIT-safe),
  zero startup cost, ~hundreds of bytes vs YSI's ~69KB framework, portable
  plain `.amx`.
- **Where YSI still wins (deferred to the runtime companion, not the
  compiler):** (1) runtime add/remove/replace of hooks
  (`DEFINE_HOOK_REPLACEMENT`) — our chain is fixed at compile time; (2) unified
  chain-control ACROSS `.amx` boundaries — ours controls the chain within each
  script, cross-script order follows open.mp's `CallInSides`. Both are runtime
  concerns; they fit the two-pillar companion if ever needed.

## Tests
`hook_order`, `hook_stop`, `hook_stop1`, `hook_continue0`, `hook_single`,
`hook_args` (ref + array forwarding), `hook_reject_sig` (arg-count mismatch),
`hook_reject_shape` (arg-shape mismatch) — all green.

## What is next
- Optional: fold single-hook callbacks to emit the body directly (skip the
  one-call dispatcher) if the extra call is ever measured to matter.
- Runtime hook add/remove/replace + cross-script chain control → companion
  plugin territory (two-pillar), if demanded.
- Tag mismatch across hooks is not diagnosed (harmless: forwarding is by
  slot/address); add if a use case appears.
