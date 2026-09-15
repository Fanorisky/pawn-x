# Varargs Forwarding (`___`) — Native Compiler Feature Design

**Status:** Approved design (in-session review, 2026-09-15)
**Experiment:** pawn-x experiment 001
**Upstream:** openmultiplayer/compiler (vendored at `compiler/`)
**Replaces:** YSI `y_va` runtime bytecode rewriting

## 1. Problem

Pawn has no way to forward the variable arguments of a variadic script
function to another call. The declaration side works today — a script
function may declare `...` — but inside the function body there is no
language construct meaning "pass my remaining arguments along". Script
authors need this for wrappers around `format`, `printf`,
`CallLocalFunction`, `SetTimerEx`, and similar natives.

YSI's `y_va` fills this gap with `___` (triple underscore): a runtime
system that scans and rewrites the compiled `.amx` bytecode at first use
(`impl.inc:481-623`), then manipulates stack frames with hand-written
assembly (`YVA2_DoPush`, `impl.inc:87-190`). The author's own comments
call it "by far the trickiest method", it depends on exact compiler
optimization output patterns, breaks under recursion, and caps nesting
at `MAX_NESTED_PASSTHROUGHS` (4).

The compiler knows everything y_va guesses at runtime: the frame layout,
the argument count location, and the call target. This feature moves the
mechanism into codegen.

## 2. Feature

A `___` token in a call's argument list forwards all variable arguments
received by the enclosing variadic function:

```pawn
MyPrintf(const fmat[], ...)
{
    printf(fmat, ___);          // forward everything after fmat
}

Log(level, const fmat[], ...)
{
    MyPrintf2(fmat, ___);       // forwarding chains through wrappers
}
```

`___(N)` skips the first N *named* parameters before forwarding
(mirrors y_va's `va_start<N>`):

```pawn
Wrapper(first, const fmat[], ...)
{
    format(dest, size, fmat, ___(2));   // skip `first` and `fmat`
}
```

## 3. Semantics

| Aspect | Decision | Rationale |
|---|---|---|
| Syntax | bare `___` = skip 0; `___(N)` = skip N named params | Exact meaning of `va_start<N>` / `___N` (y_va.inc:114-123) |
| Valid location | Only inside a call's argument list; the enclosing function MUST be variadic (declares `...`) | Every y_va test uses `___` inside a `va_args<>` function |
| Position in target call | Only at the position of the target's first variadic argument (after manually-filled named arguments) | y_va always forwards at the tail; mid-list forwarding has no use case |
| Argument count | Computed at run time from the enclosing frame's byte count (`*(frm+2*cell)`); NOT a compile-time constant | Recursion and multiple call sites make the count dynamic; y_va's DoPush reads it at runtime (`LOAD.S.pri 8`) |
| Type checking | Skipped for forwarded arguments | The enclosing function's tags were checked on entry; y_va uses `GLOBAL_TAG_TYPES` catch-all |
| Forwarding mode | Cell-by-cell by value, copied from the caller's frame to the new call's stack | y_va copies stack-to-stack via `memcpy`; no use case needs by-reference |
| Nested `___` | Supported: `Func1(Func2(___), ___)` — each `___` reads its own enclosing frame | Tested explicitly by y_va (`y_va2_Nesting`) |
| String return (`va_return`) | Out of scope | Separate language gap (functions cannot return strings); do not couple |
| Error case | `___` inside a non-variadic function is a compile error (new error number 253 — the current maximum in `sc5.c` is 252) | Today `___` is an invalid token there; opt-in, no silent behavior |

## 4. Technical Design

### 4.1 Compile-time operands

When parsing a call inside variadic function `F`:

- `skip` = N from `___(N)` — a compile-time constant.
- `src_offset` = `(skip + named_params(F) + 3) * cell` — address of the
  first forwarded argument, relative to `F`'s frame pointer.
- `cell_count` = NOT known at compile time. Read at run time from `F`'s
  frame: `*(frm + 2*cell)/cell - skip - named_params(F)`.

### 4.2 Codegen

Emitted in `sc3.c` `callfunction()`, new branch for the forwarded-varargs
argument. Two implementation options:

**Option A (chosen first): inline copy loop.** ~20 instructions using only
existing AMX opcodes: compute source address (`LCTRL 5`, `ADD.C`), compute
count (`LREF` from `frm+2*cell`, subtract), loop `LOAD.I / PUSH.pri /
ADD.C 4 / EQ / JZER`. Self-contained; no host dependency.

**Option B (later, if benchmarks demand): reuse `memcpy` native.** Shorter
sequence (stack adjust via `SCTRL 4` + `SYSREQ.C memcpy`), exactly what
`YVA2_DoPush` does. Requires the host to provide `memcpy` (open.mp does).
Measurable upgrade path — this is a lab; benchmarking is the point.

### 4.3 The hard part: dynamic byte count at the call site

`callfunction()` currently emits `pushval(nargs*sizeof(cell))`
(sc3.c:2670) — a constant. A call containing `___` needs:

```
byte_count = static_nargs*4 + *(frm+2*cell) - (skip + named_params)*4
```

`PUSH.C` takes only constants, so this must be computed into PRI
(`LCTRL 5`, `LREF`, arithmetic) and emitted as `PUSH.pri`. The peephole
optimizer (`sc7.c`) assumes the `push.c` before-call pattern and must be
verified not to mis-optimize the dynamic form.

### 4.4 Compiler changes

| File | Change | Size |
|---|---|---|
| `sc2.c` lexer + `sc.h` | Token for `___`, recognized in argument context | small |
| `sc3.c` `callfunction()` | New case: validate enclosing function is variadic, compute `skip`/`src_offset`, emit §4.2 sequence, mark target's variadic slot dynamically filled | medium (core) |
| `sc3.c` final count | Dynamic byte-count emission instead of `pushval` constant for calls containing `___` (§4.3) | hard |
| `sc1.c` `declargs()` | Unchanged (script `...` declarations already work); possibly a clearer diagnostic referencing `___` | small |
| `tests/` | `varargs_forward_*.pwn` + `.meta` pairs | medium |

### 4.5 Safety & compatibility

- No new opcodes — the emitted sequence uses only existing AMX
  instructions, so unmodified open.mp servers can run the output.
- Opt-in: `___` is not valid syntax in that position today, so no
  existing script changes meaning.
- The runtime contract that makes this safe: `RETN` reads the argument
  byte count from the frame dynamically (amxexecn.asm `OP_RETN`:
  `mov ebp,[edi+ecx]; lea ecx,[ecx+ebp+4]`), so a variadic callee cleans
  up whatever count it was actually given.
- Public functions receiving host-provided dynamic arguments
  (`CallLocalFunction` passthrough, y_va test `y_va_CallLocalFunction`)
  get dedicated tests.

## 5. Test Plan

Translated one-to-one from the y_va test suite:

| Test | Provenance (y_va test) |
|---|---|
| `varargs_forward_basic` — passthrough to `printf` | `y_va2_printf` |
| `varargs_forward_skip` — `___(N)` for N = 0..3 | `y_va_printf_0..3` |
| `varargs_forward_nested` — `Func1(Func2(___), ___)` | `y_va2_Nesting` |
| `varargs_forward_format` — `format()` output identical (`"Hello 99 001F woop"`) | `y_va2_format` |
| `varargs_forward_dynamic` — `CallLocalFunction` passthrough | `y_va_CallLocalFunction` |
| `varargs_forward_reject` — `___` in non-variadic function → exact new error | (new, per §3 error case) |

Plus a differential test: the same script compiled with y_va and with
native `___` must produce identical output.

## 6. Out of Scope (YAGNI)

- `va_return` / functions returning strings — separate gap.
- Mixing named parameters with `___` in the target call.
- `___` at mid-list positions of the target call.
- Tail-forwarding optimization (skip=0 wrapper that only forwards).

## 7. Success Criteria

1. All six test files above pass against our modified `pawncc` + the
   upstream test suite (94 tests) still passes — zero regressions.
2. The differential test shows identical behavior to y_va for the
   forwarded-argument use cases.
3. No new opcodes: output runs on an unmodified open.mp server.
4. `docs/experiments/001-varargs/RESULT.md` documents what was tried,
   what worked, what broke, and what is next.
