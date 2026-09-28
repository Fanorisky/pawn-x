# Native `task` / `ptask` — REAL open.mp host validation

**Date:** 2026-09-28  **Branch:** `fix/ptask-public` (fix on top of `feat/native-task` + `feat/ptask`)  **Server:** open.mp 1.5.8.3079, headless

## Question
Do native `task` (auto-registered repeating timer) and `ptask` (per-player)
actually fire on a **real open.mp server**? The in-harness tests use `pawnruns`,
which has no SA-MP `SetTimer` / player natives, so registration+firing could only
be validated live.

## Answer: YES — both fire. And the host run caught a real bug the harness missed.

## Reproduce
```
# config.json: "pawn": { "legacy_plugins": ["iterset.so"], "main_scripts": ["task_host 1"] }
# (iterset.so provides the set* natives that <players>/<ptask> use)
build/pawncc openmp/Server/gamemodes/task_host.pwn \
    -iopenmp/Server/qawno/include -icompiler/include \
    -oopenmp/Server/gamemodes/task_host.amx
cd openmp/Server && timeout -s INT -k 2 3 ./omp-server
```

Gamemode (`task_host.pwn`):
```pawn
#include <open.mp>
#include <ptask>

task Beat[300]() { print("[task] beat"); }
ptask PlayerTick[300](playerid) { printf("[ptask] player %d", playerid); }

hook OnGameModeInit()
{
    setadd(Player, 0);                       // inject id 0 so ptask has a player
    printf("[host] Player len=%d", setlen(Player));
    return 1;
}
```

## Observed (after the fix)
```
[Info] [host] Player len=1
[Info] [task] beat        x9   (every 300ms over ~3s)
[Info] [ptask] player 0   x9
```
`task` fires on a real `SetTimer`; `ptask` fires once per connected player each
interval (id 0, injected). The compiler-synthesised registration
(`SetTimer("@yt_Beat", …)` and `SetTimer("@ptd_PlayerTick", …)`, chained onto
`OnGameModeInit` via the hook machinery) works end-to-end, no plugin for the
timers themselves.

## The bug the host caught (in-harness tests all passed)
Before the fix, `ptask` fired **0** times while `task` fired fine. Root cause:
the synthesised per-player dispatcher `@ptd_Name` was flagged `uPUBLIC|uFORWARD`
but **not `uDEFINE`**. The publics table only lists `uPUBLIC && uDEFINE` symbols
(sc6.c:784), so `@ptd_Name` was silently **absent from the publics table** ->
`SetTimer("@ptd_PlayerTick", …)` could not resolve the callback -> the per-player
timer never fired. `task` was unaffected (its `@yt_Name` body is created by
`newfunc`, which sets `uDEFINE`).

Neither the `ptask_native` pcode-check (the dispatcher *code* was emitted, just
not exported) nor the `ptask_dispatch` runtime test (it calls `__ptask_dispatch`
directly, bypassing name resolution) could see this — only a live `SetTimer`
resolving the name by the publics table exposed it.

**Fix:** flag `@ptd_Name` `uREAD|uPUBLIC|uDEFINE|uPROTOTYPED` (mirroring
`hook_emit_dispatchers`), so it is exported and `SetTimer` resolves it.
RED (pre-fix): `[ptask]` fires 0. GREEN (post-fix): fires 9.

## Note
The `[Error] Invalid index parameter (bad entry point)` line at startup is a
pre-existing open.mp quirk for a minimal gamemode with no `main()` — it appears
with a bare `hook OnGameModeInit()` and no timers at all, and is unrelated to
`task`/`ptask`.
