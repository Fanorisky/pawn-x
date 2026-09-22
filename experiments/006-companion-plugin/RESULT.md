# Experiment 006: Companion Plugin — the runtime pillar

**Date:** 2026-09-22
**Server:** open.mp `omp-server` (real runtime)
**Closes:** the one gap exp 005 found — runtime hook add/remove/replace
(YSI `DEFINE_HOOK_REPLACEMENT`), which a compiler cannot own.
**Plan:** two phases (user choice "A lalu B"). **Both phases DONE & proven live.**

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

## Phase B — transparent built-in-callback interception (shipped, portable)

Make runtime chains fire **automatically** on real callbacks — matching
`DEFINE_HOOK_REPLACEMENT` — without the script wiring `dynhook_call` anywhere.
Per the user's choice this is done the **portable** way (works on SA-MP
`samp03svr` *and* open.mp), NOT as an open.mp-only SDK component:

- **Mechanism: inline-hook the real `amx_Exec`** via `subhook` (Zeex's inline
  hooking lib; upstream `Zeex/subhook` is gone, use the `Dasharo/subhook` or
  `tianocore/edk2-subhook` mirror). This is the same technique crashdetect /
  sampgdk use, so it needs **no open.mp SDK** and stays SA-MP compatible. The
  plugin gets `amx_Exec`'s address from the exports table, patches its
  prologue, and routes every public dispatch through `Exec_hook`.
- **`dynhook_intercept(const callback[])`** marks a callback name. On `AmxLoad`
  the plugin builds a per-AMX public-index → name table (`amx_NumPublics` +
  `amx_GetPublic`), so `Exec_hook` can map an incoming call index back to a name.
- On a marked call: capture the args (`paramcount` cells at `STK`), run the
  original body unchanged, then dispatch the runtime chain with the same args
  (fresh push+exec per handler). All internal dispatch uses the subhook
  **trampoline** (never re-entering the hook); a guard covers nested host calls.

### Live proof (`dyntest_b.pwn`, on the real server)

```
Loading plugin: dynhook
[B] fire#1 no handlers -> original only
  OnThing.original a=1
[B] fire#2 +ExtraA +ExtraB -> original + both (OnThing wired nothing)
  OnThing.original a=2
  ExtraA a=2                   <- handlers fire TRANSPARENTLY; OnThing wired nothing
  ExtraB a=2
[B] fire#3 removed ExtraA -> original + ExtraB
  OnThing.original a=3
  ExtraB a=3                   <- runtime REMOVE took effect
[DONE] dynB
```

`OnThing`'s body never calls `dynhook_call`, yet handlers registered at runtime
fire when it runs, and `dynhook_remove` takes effect between calls — full
transparent interception, portable across both hosts, no bytecode rewriting.

### Honest limits of Phase B v1

- **Post-hook only**: handlers run *after* the original and cannot suppress it
  (no pre-hook chain-`STOP` yet). This is deliberate — it avoids live AMX-stack
  surgery (re-pushing over the host's frame + `paramcount` juggling), the part
  that most easily corrupts the VM. Pre-hook + stop-control is a v2 refinement.
- Handlers resolve in the **same AMX** that fired the callback (the common
  case; the host already dispatches callbacks to every script anyway).
- `subhook` inline-hooks `amx_Exec` process-wide; coexisting with other plugins
  that also hook `amx_Exec` is the usual SA-MP caveat (subhook trampolines
  chain, but ordering across plugins is not guaranteed).

## Portability summary (why "B portable" was chosen)

| layer | samp03svr | open.mp |
|---|---|---|
| compiler features (`___`, `set_foreach`, `iterfunc`, `hook`) | ✅ plain AMX, no new opcodes, `sNAMEMAX`=31 | ✅ |
| `iterset` / `dynhook` natives (legacy plugin ABI) | ✅ | ✅ |
| Phase B interception (`subhook` on `amx_Exec`) | ✅ classic technique | ✅ |
| (rejected) Phase B as open.mp SDK component | ❌ no component system | ✅ only |

The whole pawn-x stack now runs on **both** hosts from one build.

## Files
`dynhook.cpp` (Phase A + B, tracked), `dyntest.pwn` (Phase A proof),
`dyntest_b.pwn` (Phase B proof), `compiler/include/dynhook.inc`. Built `.so`
lives under gitignored `openmp/Server/plugins/`; `deps/subhook/` is likewise
gitignored (clone a mirror). Rebuild:
```
gcc -m32 -fPIC -DSUBHOOK_STATIC -c deps/subhook/subhook.c -o /tmp/subhook.o
g++ -m32 -shared -fPIC -DLINUX -DSUBHOOK_STATIC -Icompiler/source/amx \
  -Icompiler/source/linux -Ideps/subhook \
  experiments/006-companion-plugin/dynhook.cpp /tmp/subhook.o \
  -o openmp/Server/plugins/dynhook.so
```
