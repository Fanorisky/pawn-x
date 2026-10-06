# Native `task` / `ptask`: REAL open.mp host validation

**Date:** 2026-09-28  **Branch:** `fix/ptask-public` (fix on top of `feat/native-task` + `feat/ptask`)  **Server:** open.mp 1.5.8.3079, headless

## Question
Do native `task` (auto-registered repeating timer) and `ptask` (per-player)
actually fire on a **real open.mp server**? The in-harness tests use `pawnruns`,
which has no SA-MP `SetTimer` / player natives, so registration+firing could only
be validated live.

## Answer: YES, both fire. And the host run caught a real bug the harness missed.

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
directly, bypassing name resolution) could see this: only a live `SetTimer`
resolving the name by the publics table exposed it.

**Fix:** flag `@ptd_Name` `uREAD|uPUBLIC|uDEFINE|uPROTOTYPED` (mirroring
`hook_emit_dispatchers`), so it is exported and `SetTimer` resolves it.
RED (pre-fix): `[ptask]` fires 0. GREEN (post-fix): fires 9.

## Note
The `[Error] Invalid index parameter (bad entry point)` line at startup is a
pre-existing open.mp quirk for a minimal gamemode with no `main()`: it appears
with a bare `hook OnGameModeInit()` and no timers at all, and is unrelated to
`task`/`ptask`.

---

# Round 2 (2026-10-06): full timer matrix on open.mp 1.5.8.3134 + 2 more bugs

Re-validated after the VPS migration, this time a full matrix (`timer_matrix.pwn`)
driven on the real host, with REAL connected NPCs (`NPC_Create`) for `ptask`
fan-out, plus a controlled isolation ladder (`npc_probe`, `iso*`, `defer_expr`).
Found and fixed two more silent-failure bugs the earlier narrow run missed.

## Bug A: task/ptask auto-registration stripped when SetTimer not otherwise used

`task`/`ptask` lower to a synthesised `@yt_init` that CALLS the `Timer_Set` stock
(bound to the host `SetTimer` by `<timers>`). `@yt_init` is created at
end-of-parse, AFTER dead-code elimination. In a script that uses ONLY
`task`/`ptask` (no `defer`, no manual `SetTimer`), the `Timer_Set` stock had no
other referrer at DCE time, so it was dropped; `@yt_init`'s call then resolved to
a wrong address and `SetTimer` was never issued -> the timer silently never
fired (compile clean, no warning).

Why Mode never hit it: Mode uses `defer`/`repeat` (48 sites) + many `task`/`ptask`,
so `Timer_Set`/`Timer_SetEx` always had live referrers. Proven: Mode `GlobalTick`
fires; a `task`-only gamemode fires 0.

**Fix (sc1.c `dotask`):** mark `Timer_Set` (and, for ptask, `__ptask_dispatch`)
`uREAD` as soon as a `task`/`ptask` is parsed, mirroring how the `@yt_Name` body
is already kept. RED: `task`-only fires 0 / no `sysreq SetTimer`. GREEN: fires
every interval. Regression: `tests/task_only_registers`.

## Bug B: defer/repeat argument that is an expression with a constant operand

`defer F[ms](n + 1)` compiled to `1 + 1` (always 2), not `n + 1`.
`emit_timer_schedule` parsed each argument with `expression()` WITHOUT
stage-buffering, so plnge2()'s `stgdel()` (which scratches the pushed left
operand when the right operand is a constant) was a no-op: the pushed `n` was
orphaned (warning 215 "expression has no effect" + a stack leak) and ALT was
loaded with the constant. Same bug class as the yield-return path.

Why Mode never hit it: all 48 Mode `defer`/`repeat` arg lists are plain
variables/constants; the one expression (`strlen(text)*60`) is in the `[interval]`
slot, which uses the runtime-stash path, not the arg path.

**Fix (sc1.c `emit_timer_schedule`):** wrap the whole body in local staging
(`stgset(TRUE)`/`stgout`), exactly like the yield-return emitter. RED:
`defer Step(n+1)` yields 1,2,2,2,... (stuck). GREEN: 1,2,3. Regression:
`tests/defer_expr_arg`. This also fixes the latent interval-expression case.

## Matrix result (all GREEN, host-validated)
task (auto-reg) fires every interval; ptask fans out over REAL NPCs and shrinks
correctly on mid-run connect/disconnect; `defer` one-shot fires exactly once;
`defer` arg forwarding int/float/string correct (`i=42 f=3.14 s=hello`);
`repeat`+arg repeats; self-rescheduling `defer F(n+1)` counts 1,2,3 and stops;
`stop`/`Timer_Stop` halts a running timer. No crashes, no runtime errors.
