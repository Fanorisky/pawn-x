# `iterfunc` Generators — Experiment 003 Phase 3 Design

**Status:** Design (in-session, 2026-09-19)
**Experiment:** pawn-x experiment 003, phase 3
**Depends on:** `set_foreach` keyword (exp 002), phases 1-2 (setfree/setrandom, Reverse)

## 1. Problem

`set_foreach` currently iterates a *materialised* compact set (an array the
`set*` natives maintain). YSI also offers **lazy generators** via `iterfunc`:
a function called once per iteration that computes the next value on the fly,
with no backing array. Examples (YSI `y_foreach/iterators.inc`):

```pawn
iterfunc stock Range[cellmin](cur, min, max, step = 1) { ... }
foreach (new i : Range(0, 10))     // 0,1,...,9 — nothing stored
```

This gives filtered/computed/infinite sequences (`Range`, `Powers`, `Fib`,
`Filter`, ...) without allocating. Native pawn-x has no equivalent yet.

## 2. Feature

A function declared `iterfunc` is a **generator**: given the current value it
returns the next one, or a sentinel to stop. `set_foreach` over a call to an
iterfunc emits a call-loop instead of an array walk.

```pawn
iterfunc Range(cur, lo, hi)
{
    if (cur == ITER_STOP) return lo;   // first call: seed
    if (cur + 1 < hi)      return cur + 1;
    return ITER_STOP;                  // done
}

main()
{
    set_foreach (new i : Range(0, 5))  // 0 1 2 3 4
        printf("%d\n", i);
}
```

## 3. Decisions (to lock)

| Aspect | Proposal | Rationale |
|---|---|---|
| Declaration | keyword **`iterfunc`** before the function (like YSI) | explicit, greppable; sets a symbol flag `uITERFUNC` |
| Generator contract | first param is `cur` (running state); returns next value, or `ITER_STOP` to end | mirrors YSI's `cur`/sentinel protocol |
| Sentinel | **`ITER_STOP` = `cellmin`** (0x80000000), predefined constant | YSI uses `cellmin`; a value scripts never legitimately iterate |
| Seed (first call) | `cur == ITER_STOP` on the first call; the iterfunc returns the first value (or `ITER_STOP` for an empty sequence) | one uniform protocol, no separate "init" entry |
| Extra args | evaluated **once** at loop entry, cached in hidden cells, passed unchanged every call; only `cur` varies | args are loop-invariant (YSI semantics); avoids re-evaluating side effects |
| `set_foreach` dispatch | operand is a **call to an `iterfunc` symbol** → call-loop codegen; operand is an array-ref → existing pointer walk; anything else → error 255 | one keyword, two operand kinds |
| Reverse | `Reverse(...)` NOT supported over an iterfunc (generators define their own order) — error | a generator has no random access to reverse |
| break/continue | supported, via the existing `dofor` wq machinery | same as the array walk |
| Loop var | bound to each returned value; scoped to the loop | same as array walk |

## 4. Technical design

### 4.1 Declaration
- New keyword `iterfunc` (token `tITERFUNC`), recognised where a function
  declaration starts. Sets `sym->usage |= uITERFUNC` (new flag bit in `sc.h`).
- Otherwise a normal function: `cur` is an ordinary first parameter; the body
  is ordinary code. No special return handling at declaration — the protocol
  is a convention the `set_foreach` call-loop relies on.
- `ITER_STOP` predefined as a constant (`cellmin`) so scripts can name it.

### 4.2 `set_foreach` operand recognition
In `doforeach`, after `:` (and after the optional `Reverse(` check), peek: is
the next token a symbol that resolves to a function with `uITERFUNC`, followed
by `(`? If yes → **generator path**; else fall through to
`parse_foreach_operand` (array path).

If `Reverse(` was seen AND the operand is an iterfunc → error (reverse over a
generator is undefined).

### 4.3 Generator codegen
For `set_foreach (new i : Gen(a, b))`:
```
; evaluate extra args a,b ONCE, cache in hidden cells arg1,arg2
; cur = ITER_STOP
loop_cond:
  ; call Gen(cur, arg1, arg2): push args right-to-left, push cur, push argcount, call
  ; return value (next) in PRI
  ; if (PRI == ITER_STOP) goto exit
  stor cur = PRI
  stor i   = PRI      ; loop variable
  BODY
continue:
  goto loop_cond      ; cur already updated; args cached
exit:
```
The per-iteration call reuses the compiler's function-call emission
(`ffcall`/`pushval`), with `cur` + the cached args as arguments. Argument
count and order match the iterfunc's declared parameters.

### 4.4 Compiler changes
| File | Change | Size |
|---|---|---|
| `sc.h` | `tITERFUNC` token, `uITERFUNC` usage flag | small |
| `scvars.c` | `"iterfunc"` in `sc_tokens[]` (aligned) | small |
| `sc1.c` | recognise `iterfunc` at function start → set flag; `doforeach` generator path + call-loop codegen | medium-hard |
| `sc5.c` | reuse error 255 (reverse-over-generator, non-iterable) | small |
| `compiler/include/foreach.inc` | `const ITER_STOP = cellmin;` | small |
| `tests/` | `iterfunc_range`, `iterfunc_filter`, `iterfunc_empty`, `iterfunc_break`, reject reverse-over-iterfunc | medium |

### 4.5 Safety & compatibility
- Opt-in: `iterfunc` is not valid syntax today; `set_foreach` generator path
  only triggers for a call to a `uITERFUNC` symbol.
- No new AMX opcodes: the call-loop uses ordinary `call`/`push`/`retn`.
- An `iterfunc` is a normal function too — callable directly (returns one step);
  only `set_foreach` gives it loop semantics.

## 5. Test plan
| Test | Behavior |
|---|---|
| `iterfunc_range` | `Range(0,5)` → 0 1 2 3 4 |
| `iterfunc_step` | a generator with a step arg (args cached once) |
| `iterfunc_filter` | generator emitting only values passing a predicate |
| `iterfunc_empty` | generator returns `ITER_STOP` first call → body never runs |
| `iterfunc_break` | `break` stops a generator loop |
| `iterfunc_reject_reverse` | `set_foreach (i : Reverse(Range(0,5)))` → error |

## 6. Out of scope (YAGNI)
- `yield`/coroutine-style generators (separate, needs frame suspension).
- Generators that take the loop var by reference / bidirectional.
- Infinite generators without `break` (allowed, user's responsibility).

## 7. Success criteria
1. All §5 tests pass; upstream suite no regressions (2 known baselines only).
2. No new opcodes; runs on unmodified open.mp.
3. `experiments/003-*/RESULT.md` documents the generator protocol + results.
