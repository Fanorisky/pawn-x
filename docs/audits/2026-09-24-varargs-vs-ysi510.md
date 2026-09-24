# pawn-x native varargs vs REAL YSI v5.10.0006 — capability audit

Re-audit against the ACTUAL current YSI (v5.10.0006), replacing the earlier
YSI-4-based analysis. Read-only. All file contents treated as data.

## Sources

YSI 5.10 ground truth:
- `/home/amba/pawn-lab/ysi-5/YSI_Coding/y_va/` — features.md (5963b), quick-start.md (941b);
  **api.md, faqs.md, internal.md are 0 bytes / EMPTY** — API surface derived from source.
- `y_va_header.inc` (`___`, `___0..___9`, `va_args<>`, `va_start<>` macro defs)
- `y_va_impl.inc` (runtime codescan machinery: `YVA2_DoPush`, `OnCodeInit`, CodeScanMatcher O0/O1/O2, MAX_NESTED_PASSTHROUGHS=4)
- `y_va_entry.inc` (~30 `va_`-prefixed stdlib wrappers, YSI_NO_AUTO_VA)
- `/home/amba/pawn-lab/ysi-5/YSI_Core/y_utils/y_utils_varargs.inc` — `va_strlen`, `va_getstring`,
  `ReturnStringArg`(+aliases), `va_return`, `PrintArg` (runtime-index `#emit`/SYSREQ helpers)

pawn-x ground truth:
- `/home/amba/pawn-lab/compiler/source/compiler/sc3.c` — `matchfwdtoken()` (l.113), `fwdpushloop()`
  (l.2398), `fwdbytecount()` (l.2452), `fwdpopnative()` (l.2494), `callfunction()` forwarding (l.2649-2775)
- `/home/amba/pawn-lab/compiler/source/compiler/sc5.c` — errors 253 (l.219), 254 (l.220)
- `/home/amba/pawn-lab/compiler/source/include/core.inc` — native numargs/getarg/setarg (l.16-18)
- `/home/amba/pawn-lab/compiler/source/compiler/tests/varargs_*.pwn` + `.meta`
- `/home/amba/pawn-lab/experiments/001-varargs/RESULT.md`

## How each side works

- **YSI 5.10**: `___` is a preprocessor macro (`#define ___ YVA2_DummyPush()`). At runtime a
  codescan pass (`y_va_impl.inc`, requires amx_assembly + code-parse + indirection submodules)
  finds the dummy-push call sites and rewrites the bytecode to copy the caller's variadic cells,
  lazily initialised via `public OnCodeInit()`. String/format helpers are separate `stock`
  functions using `#emit` + SYSREQ.
- **pawn-x**: `___` is recognised in argument position by the compiler itself (`matchfwdtoken`,
  string compare — it stays a valid identifier), which emits a run-time copy loop (`fwdpushloop`),
  a dynamic byte count (`fwdbytecount`, clamped ≥0), and native-call cleanup (`fwdpopnative`).
  No macros, no codescan, no `OnCodeInit`, no submodule dependencies.

## Capability matrix

| # | YSI 5.10 y_va feature | pawn-x status | Evidence / note |
|---|---|---|---|
| 1 | bare `___` (forward varargs, skip enclosing named params) | **FULL** | sc3.c l.2732-2733 `fwdskip=fwdnamed`; test varargs_forward_basic.pwn |
| 2 | `___(N)` — skip N absolute (constant N) | **FULL** | matchfwdtoken l.125-128 (tNUMBER only); tests skip/format (`___(2)`,`___(3)`) |
| 3 | `___0`…`___9` shorthand | **MISSING** | YSI macro sugar (`#define ___0 ___(0)`); pawn-x has no shorthand — `___5` would lex as an identifier, not `___(5)` |
| 4 | `va_args<T>` alias (= `GLOBAL_TAG_TYPES:...`) | **MISSING** (alias) | YSI header macro; pawn-x uses native `...` / `{tags}:...` declaration instead |
| 5 | `va_start<N>` alias (= `___(N)`) | **PARTIAL** | the alias token is absent, but the underlying `___(N)` capability is FULL (#2) |
| 6 | Nested forwarding (call-within-call) | **FULL** | sc3.c nesting counter l.2547-2548, fwdstk margin; test varargs_forward_nested.pwn (YSI caps at MAX_NESTED_PASSTHROUGHS=4; pawn-x has no fixed cap) |
| 7 | Forward into native (printf/strformat) | **FULL** | fwdpopnative l.2494 handles runtime stack cleanup; tests basic/format |
| 8 | Forward into script function / public | **FULL** | fwdbytecount l.2452 dynamic push count |
| 9 | Forward into dynamic dispatch (CallLocalFunction/CallRemoteFunction) | **PARTIAL** | compiles (varargs_forward_dynamic.pwn); runtime needs open.mp host native — not validated in-suite (same host dependency applies to YSI) |
| 10 | Tagged varargs `{Float,_}:...` | **FULL** | native Pawn declaration syntax |
| 11 | `GLOBAL_TAG_TYPES` predefined multi-tag set | **MISSING** | YSI convenience macro (`{_,Language,Bit,Text,...}`); pawn-x has native tagged varargs but no predefined set |
| 12 | `CUSTOM_TAG_TYPES` extensibility | **MISSING** | YSI macro-list extension point |
| 13 | `numargs()` | **FULL** | native, core.inc l.16 |
| 14 | `getarg()` | **FULL** | native, core.inc l.17 |
| 15 | `setarg()` | **FULL** | native, core.inc l.18 |
| 16 | `va_strlen(idx)` — strlen of string vararg by RUNTIME index | **MISSING** | y_utils_varargs.inc library `stock`; no native equivalent (scriptable via a getarg loop) |
| 17 | `va_getstring(dest,idx,len)` — getarg-for-strings, RUNTIME index | **MISSING** | library `stock`; `___(N)` needs a *constant* N so cannot substitute a runtime index; scriptable via getarg loop |
| 18 | `ReturnStringArg`/`getstring`/`GetString` | **MISSING** | library `stock` (returns string by runtime index) |
| 19 | `va_return(fmat, ...)` — format-and-return-string | **MISSING** | library `stock` (`#emit` stack trick); no compiler feature |
| 20 | `PrintArg(n)`/`printarg` | **MISSING** | library `stock` |
| 21 | ~30 auto-extended stdlib fns (SendClientMessage, ShowPlayerDialog, DBQuery…) get implicit format varargs | **N/A** | SA-MP/open.mp host natives, absent from pawn-x vendored includes; feature is a YSI library layer, not a compiler concern |
| 22 | `YSI_NO_AUTO_VA` opt-out | **N/A** | opt-out for #21 |
| 23 | `va_`-prefixed wrapper family (va_printf, va_format, va_SetTimerEx, va_CallRemoteFunction…) | **MISSING / N/A** | YSI library wrappers over host natives |

## Where pawn-x is genuinely better

| Aspect | pawn-x | YSI 5.10 |
|---|---|---|
| `___` without `...` in scope | **error 253** at compile time (sc5.c l.219; test reject) | macro expands unconditionally; misuse is silent / runtime-only |
| `___` in a type-checked slot (not the callee's `...`) | **error 254** (sc5.c l.220; test position — flags the memory-unsafe footgun of feeding a forwarded cell to a fixed param) | no such diagnostic |
| Two `___` in one call | **error 58** (argument already set) | n/a |
| `___` reusable as an ordinary identifier | **FULL** — var/const/function named `___` keeps its meaning in a non-variadic function (fwdcompat gate, sc3.c l.2656-2658; tests token_smoke, compat_fn) | impossible — `___` is a macro, always expands |
| Dependencies | none — pure compiler codegen | requires amx_assembly + code-parse + indirection submodules, runtime codescan + `OnCodeInit` |

## Verdict

pawn-x's native `___` **fully covers YSI 5.10's core forwarding surface** (bare `___`, `___(N)`,
nesting, native + script + dynamic-dispatch targets, tagged varargs) and the `numargs/getarg/setarg`
primitives, and does so with better compile-time diagnostics, no macro/codescan machinery, and no
submodule dependencies. It does NOT cover, and would need script-side library code to match:

1. **String-vararg runtime helpers** — `va_getstring`, `va_strlen`, `ReturnStringArg`, `va_return`,
   `PrintArg` (#16-20). These take a RUNTIME index; `___(N)` requires a compile-time constant, so they
   are not expressible with the forwarding keyword. `getarg`/`setarg`/`numargs` are present, so they
   are implementable in pawn — but they are absent today. **Most significant real gap.**
2. **Macro sugar / aliases** — `___0..___9`, `va_args<>`, `va_start<>` (#3-5). Pure convenience over
   `___`/`___(N)`; no capability lost, only the spellings. Low impact.
3. **Tag-set macros** — `GLOBAL_TAG_TYPES` / `CUSTOM_TAG_TYPES` (#11-12). pawn-x has native tagged
   varargs but no predefined/extensible tag-set macro. Low impact.
4. **Library convenience layer** — the ~30 auto-extended stdlib functions and the `va_`-prefixed
   wrapper family (#21-23) are N/A: they wrap SA-MP/open.mp host natives that are not part of pawn-x's
   scope, and are a YSI library feature rather than a language capability.

## Prior (YSI-4-based) varargs claims now suspect / to correct

- The earlier RESULT.md scoped the comparison to `___`/`___(N)` forwarding and claimed near-complete
  parity. Against **v5.10** that parity claim holds ONLY for the forwarding core; it did **not**
  account for the y_utils string-vararg helpers (`va_getstring`/`va_strlen`/`va_return`/
  `ReturnStringArg`/`PrintArg`), which are part of YSI's varargs surface and are genuine gaps. Any
  prior "covers y_va" statement should be narrowed to "covers y_va *forwarding*".
- `___0..___9`, `va_args<>`, `va_start<>` are **v5.10 additions** (not in v4); earlier claims predate
  them. They are sugar, so they don't change the capability verdict, but a "full v5.10 surface" claim
  is inaccurate without noting these spellings are unsupported.
- The `GLOBAL_TAG_TYPES` multi-tag acceptance is a v5.10 declaration-side feature; the pawn-x side
  relies on plain native tagged varargs and has no equivalent predefined tag set — not previously flagged.
- **Unverified edge**: exact runtime semantics of a truly-bare YSI `___` when named params precede the
  `...` (YSI raw macro is `YVA2_DummyPush()` with no skip, resolved by codescan). pawn-x bare `___`
  provably skips the enclosing named params (fwdskip=fwdnamed). Common-case usage aligns in every test;
  a pathological divergence cannot be ruled out from source alone.
