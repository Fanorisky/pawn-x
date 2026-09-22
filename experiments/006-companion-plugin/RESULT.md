# Experiment 006: Companion Plugin — the runtime pillar

**Date:** 2026-09-22
**Server:** open.mp `omp-server` (real runtime)
**Closes:** the one gap exp 005 found — runtime hook add/remove/replace
(YSI `DEFINE_HOOK_REPLACEMENT`), which a compiler cannot own.
**Plan:** two phases (user choice "A lalu B"). **Phase A = DONE & proven live.**

## Two-pillar recap

- **Compiler pillar** (`hook`, exp 004): chain fixed at COMPILE time. Cheap,
  deterministic, no runtime cost — but immutable.
- **Companion pillar** (this plugin): the RUNTIME-only capability — mutate the
  chain while the server runs. Together they cover everything y_hooks does.

## Phase A — runtime named-event registry (shipped)

`dynhook.cpp`, an open.mp legacy plugin (built `-m32 -shared`, deployed to
`openmp/Server/plugins/dynhook.so`). Chains live in a C++
`map<string, vector<string>>`; **no bytecode scanning or rewriting** (YSI's
fragile technique) — dispatch is plain `amx_FindPublic` + `amx_Exec`.

Natives (`compiler/include/dynhook.inc`):

| native | effect |
|---|---|
| `dynhook_add(event[], pub[])` | append a handler (public name) → new count |
| `dynhook_remove(event[], pub[])` | remove first match → 1/0 |
| `dynhook_replace(event[], old[], new[])` | swap a handler in place → 1/0 |
| `dynhook_clear(event[])` | drop the whole chain |
| `dynhook_count(event[])` | handlers registered |
| `dynhook_call(event[], fmt[], ...)` | run the chain in order; `fmt` i/d/f=cell, s=string; returns handlers invoked |

`dynhook_call` snapshots the chain before dispatch, so a handler may safely
add/remove/replace within its own event. Args are forwarded per `fmt` (scalars
via `amx_Push`, strings via `amx_PushString` + `amx_Release`).

### Live proof (`dyntest.pwn`, on the real server)

```
Loading plugin: dynhook
[DYN] added H1,H2 count=2
  H1 a=7
  H2 a=7                       <- both handlers run, in registration order
[DYN] call#1 expect H1,H2 invoked=2
[DYN] removed H1 count=1
  H2 a=8                       <- runtime REMOVE took effect
[DYN] call#2 expect H2 invoked=1
[DYN] replaced H2->H3
  H3 a=9                       <- runtime REPLACE took effect
[DYN] call#3 expect H3 invoked=1
[DYN] cleared dropped=1
[DYN] call#4 expect none invoked=0
  OnMsg id=42 s=hello          <- int + string args forwarded
[DYN] call#5 string forward invoked=1
[DONE] dyn
```

Add, remove, and replace all take effect between calls — the exact
`DEFINE_HOOK_REPLACEMENT` capability, with none of YSI's runtime bytecode
rewriting. Better than YSI here: deterministic, JIT-safe, ~zero framework
weight.

### ABI

open.mp legacy plugin: `Supports() = 0x0200|0x00010000`; `Load(ppData)` reads the
AMX exports table at `ppData[16]` (indices from
`deps/open.mp/Server/Components/Pawn/main.cpp` `AMX_FUNCTIONS[]`, canonical
SA-MP order: Allot 3, Exec 7, FindPublic 9, GetAddr 13, Push 29, PushString 31,
Register 33, Release 34); `AmxLoad` registers the natives on each script.

## Boundary / honest scope of Phase A

Phase A is an **explicit** registry: you call `dynhook_call("Ev", ...)`
yourself. It is complete for **custom named events** and runtime-managed chains.
It does NOT yet transparently intercept a BUILT-IN callback (e.g. make a runtime
handler fire automatically on the real `OnPlayerConnect`). That is Phase B.

## Phase B — transparent built-in-callback interception (next)

Make runtime chains fire automatically on real open.mp callbacks, matching
`DEFINE_HOOK_REPLACEMENT` for stock events. The host-sanctioned route is an
**open.mp component** (SDK present at `deps/open.mp/SDK`, reference components at
`deps/open.mp/Server/Components`) that registers in the Pawn callback dispatch
path, rather than the legacy-plugin `amx_Exec` exports-table hook. Bigger lift
(C++ against the SDK, its own build); Phase A is the foundation it reuses.

## Files
`dynhook.cpp` (plugin source, tracked), `dyntest.pwn` (proof), plus
`compiler/include/dynhook.inc`. The built `.so` lives under the gitignored
`openmp/Server/plugins/`; rebuild with:
`g++ -m32 -shared -fPIC -DLINUX -Icompiler/source/amx -Icompiler/source/linux experiments/006-companion-plugin/dynhook.cpp -o openmp/Server/plugins/dynhook.so`
