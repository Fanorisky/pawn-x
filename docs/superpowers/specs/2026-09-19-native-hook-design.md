# Native `hook` — Single-Unit Multi-Hook (Experiment 004) Design

**Status:** Design (in-session, 2026-09-19)
**Experiment:** pawn-x experiment 004
**Replaces:** YSI `y_hooks` — the single-compilation-unit subset
**Scope decision (locked):** single-unit multi-hook, pure compiler codegen.
This is a COMPLETE solution, not a subset — see §1.1.

## 1. Problem

In Pawn a callback has exactly one implementation — a second body for the
same public function is `error 021: symbol already defined`. Scripts often
want several independent handlers for the same event (`OnPlayerConnect`,
`OnGameModeInit`, ...), e.g. one per feature module in the same gamemode.

YSI `y_hooks` provides `hook OnFoo(...) { }` allowing many bodies, chained,
with return-control. It achieves this by renaming each hook to a unique
function, riding the compiler's ALS state machinery, and — for cross-script
work — **scanning and rewriting `.amx` bytecode at runtime** (`Hooks_GetStubEntry`,
`OP_SWITCH`/`OP_CASETBL` disassembly, `y_amx` rewrites). That runtime
machinery is what this feature avoids.

## 1.1 Why single-unit is the COMPLETE fix (verified against open.mp source)

The real need behind `y_hooks` is "many handlers for one callback." That
splits in two, and open.mp already handles half of it natively:

- **Cross-`.amx` (a hook in a different filterscript/library):** open.mp's
  Pawn component ALREADY dispatches every callback to the main script AND all
  side scripts — `CallInSides` / `CallAllInSidesFirst` in
  `Server/Components/Pawn/Manager/Manager.hpp` iterate `scripts_` and `Call`
  each. So a hook in another `.amx` already fires. This is NOT missing and
  needs nothing from us.
- **Within one `.amx` (many bodies for one callback):** `amx_FindPublic`
  resolves exactly ONE public per name (`Script.cpp:amx_FindPublic_impl`), so
  Pawn rejects a second body (error 021). THIS is the gap — and it is purely a
  compile-time concern.

Therefore native `hook` (single-unit) + open.mp's existing cross-script
dispatch = full coverage of real hook usage, with no runtime companion, no
bytecode scanning, no new opcodes.

## 1.2 Better than y_hooks (honest scorecard)

Better: no runtime bytecode scan/rewrite (deterministic, not optimizer-pattern
dependent, JIT-safe), zero startup cost, ~hundreds of bytes vs YSI's ~69KB
framework, plain portable `.amx`, and a dispatcher that disassembles readably.

Where YSI still wins (deliberately deferred, both runtime concerns fit the
two-pillar companion, not the compiler): (1) runtime add/remove/replace of
hooks (`DEFINE_HOOK_REPLACEMENT`) — our chain is fixed at compile time;
(2) unified chain-control ACROSS `.amx` boundaries (ordering/break spanning
scripts) — ours controls the chain within each script; across scripts, order
follows open.mp's `CallInSides`. These are acceptable: the static-handler case
(the 99% use) is fully and better served.

## 2. Feature

```pawn
hook OnPlayerConnect(playerid)
{
    printf("A: %d", playerid);
    return HOOK_CONTINUE;      // run the next hook too
}

hook OnPlayerConnect(playerid)
{
    printf("B: %d", playerid);
    return HOOK_CONTINUE;
}
```

Every `hook OnPlayerConnect` body in the compilation unit runs, in source
order, when `OnPlayerConnect` is invoked. Return-control values decide whether
the chain continues and what the callback ultimately returns:

| Constant | Value | Meaning |
|---|---|---|
| `HOOK_CONTINUE` | 1 | run the next hook; chain result so far = 1 |
| `HOOK_CONTINUE_0` | 0 | run the next hook; chain result so far = 0 |
| `HOOK_STOP` | -1 | stop the chain, callback returns 0 |
| `HOOK_STOP_1` | -2 | stop the chain, callback returns 1 |

(Names are pawn-x's own — clearer than YSI's `Y_HOOKS_*`; not drop-in.)

## 3. Mechanism (pure codegen, no bytecode scan)

The compiler already permits multiple bodies of one public function via
**states** (sc1.c:4116 lets a redefinition through when `sym->states != NULL`),
and emits a state-dispatch stub. This feature does NOT reuse the fragile state
path; instead:

1. **Each `hook OnFoo(args)` body compiles to a uniquely-named hidden
   function** `@hook.OnFoo.<seq>(args)` (seq = source order), an ordinary
   non-public function. `hook` sets a flag so the parser accepts repeats.
2. **A registry** (compile-time, keyed by callback name) records each hidden
   hook's symbol + the shared signature.
3. **At end of compilation**, for every hooked callback, the compiler
   **synthesises one `public OnFoo(args)` dispatcher** that calls each
   `@hook.OnFoo.<seq>(args)` in order, applying the return-control protocol,
   then returns the accumulated result. This is ordinary emitted code —
   `push`/`call`/branch — no new opcodes, no runtime scanning.

### 3.1 Dispatcher shape (per callback, generated)
```
public OnFoo(a, b) {
    new r = 1;
    new h;
    h = @hook.OnFoo.0(a, b);
    if (h == -1) return 0;      // HOOK_STOP
    if (h == -2) return 1;      // HOOK_STOP_1
    r = h;                       // CONTINUE / CONTINUE_0
    h = @hook.OnFoo.1(a, b);
    ... (same per hook)
    return r;
}
```
(Emitted mechanically; the arg list is the hook's declared signature, passed
straight through.)

## 4. Technical design

### 4.1 Parser (sc1.c)
- New keyword `hook` (token `tHOOK`) at a function-declaration position.
- `hook OnFoo(args) body`: parse like a function, but
  - the real symbol name becomes `@hook.OnFoo.<seq>` (hidden, unique) so the
    duplicate-definition gate never triggers;
  - record `(OnFoo, args-signature, hidden-sym)` in the hook registry;
  - the body is ordinary code (may `return HOOK_*`).
- The signature of the first `hook OnFoo` fixes the callback's signature;
  later `hook OnFoo` with a mismatching arg count/shape → error (new number).

### 4.2 Registry + deferred emission (the hard part)
The compiler streams largely single-pass; user functions are emitted as
parsed. This feature needs **end-of-compile synthesis** of the dispatchers.
- A compile-time list of hook groups (callback name → ordered hidden syms).
- After the main parse (before finalising the code segment), walk the registry
  and emit one dispatcher public per group. This hooks into the same
  end-of-parse phase where the compiler already finalises publics/pubvars
  (sc6.c / the `sc_status==statWRITE` second pass). The dispatcher must be
  emitted in BOTH passes (statFIRST for addressing, statWRITE for code) to keep
  addresses consistent — the same two-pass discipline every function follows.

### 4.3 Constants
Predefined (in a `<hook>` header or compiler builtins):
`HOOK_CONTINUE=1`, `HOOK_CONTINUE_0=0`, `HOOK_STOP=-1`, `HOOK_STOP_1=-2`.

### 4.4 Compiler changes
| File | Change | Size |
|---|---|---|
| `sc.h` | `tHOOK` token; hook-registry structs | small |
| `scvars.c` | `"hook"` in `sc_tokens[]` (aligned) | small |
| `sc1.c` | recognise `hook`; rename to hidden sym; register; parse body | medium |
| `sc1.c`/`sc6.c` | end-of-parse dispatcher synthesis (two-pass) | **hard** |
| `sc5.c` | error for signature mismatch between hooks of one callback | small |
| `compiler/include/hook.inc` | the `HOOK_*` constants | small |
| `tests/` | multi-hook order, return-control, empty, signature-mismatch reject | medium |

### 4.5 Safety & compatibility
- Opt-in: `hook` is new syntax; existing scripts unaffected.
- No new AMX opcodes; dispatcher is ordinary calls → runs on stock open.mp.
- A callback with exactly one `hook` compiles to a dispatcher calling one
  hidden function (equivalent to a normal public, one extra call of overhead —
  acceptable; can be special-cased to emit the body directly if measured to
  matter).
- **Explicitly NOT handled:** hooks defined in a *different* `.amx`
  (filterscript/library). Combining those needs the host to route the callback
  to every script — outside a single compiler's reach. Documented as the
  boundary vs YSI.

## 5. Test plan
| Test | Behavior |
|---|---|
| `hook_order` | two hooks for one callback run in source order (both fire) |
| `hook_continue0` | `HOOK_CONTINUE_0` runs next hook, result 0 |
| `hook_stop` | `HOOK_STOP` halts the chain, later hook does NOT run, returns 0 |
| `hook_stop1` | `HOOK_STOP_1` halts, returns 1 |
| `hook_single` | one hook behaves like a normal callback |
| `hook_args` | args forwarded correctly to every hook |
| `hook_reject_sig` | mismatched signature across hooks → exact error |

Invocation in tests uses `CallLocalFunction("OnFoo", ...)` (available on the
pawnruns runtime via core natives) or a direct call to the generated public.

## 6. Out of scope (YAGNI)
- Cross-`.amx` hook chaining (host/registry territory).
- Hook priority / explicit ordering directives (source order only).
- `rehook`/replacement of an existing hook, `DEFINE_HOOK_REPLACEMENT`.
- Runtime add/remove of hooks.

## 7. Success criteria
1. All §5 tests pass; upstream suite no regressions (2 known baselines only).
2. No new opcodes; runs on unmodified open.mp.
3. `experiments/004-*/RESULT.md` documents the mechanism, the single-unit
   boundary vs YSI, and results.
