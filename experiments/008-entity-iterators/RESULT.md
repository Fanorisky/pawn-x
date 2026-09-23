# Experiment 008: Entity iterators (audit gap: ready-made Player/Vehicle/Actor)

**Date:** 2026-09-23
**Closes (partly):** the full-audit gap "YSI ships `Iterator:Player/Vehicle/Actor`
auto-wired on connect/disconnect/create/destroy; pawn-x ships none."
**Builds on:** the native `foreach` + `set*` + `hook` (exp 002/003/004).

## What shipped: `Player` iterator (`compiler/include/players.inc`)

A batteries-included connected-players set, kept in sync by hooking the callbacks
— no macros, no bytecode scan, just the native pieces:

```pawn
new Player[MAX_PLAYERS + 1];
hook OnPlayerConnect(playerid)          { setadd(Player, playerid);    return HOOK_CONTINUE; }
hook OnPlayerDisconnect(playerid, r)    { setremove(Player, playerid); return HOOK_CONTINUE; }
hook OnGameModeInit()                   { /* seed IsPlayerConnected() */ return HOOK_CONTINUE; }
```

Usage is exactly YSI's ergonomics:

```pawn
#include <players>
foreach (new i : Player) { /* every connected player */ }
```

### Live proof (`players_test.pwn`, real omp-server + iterset plugin)

Connects simulated via `CallLocalFunction` (headless server has no real players):

```
[P] connect 5,2,9
[P] foreach Player:
  2
  5
  9                 <- sorted set, maintained by the connect hook
[P] disconnect 5
[P] foreach Player:
  2
  9                 <- disconnect hook removed it
[P] len=2
[DONE] players
```

## Honest limit: Vehicle / Actor need native hooking (NOT shipped)

YSI also ships `Iterator:Vehicle/Actor`, maintained by wrapping the **native**
`CreateVehicle`/`DestroyVehicle`/`CreateActor`/... calls (YSI does this with ALS
macro redefinition of the native). pawn-x deliberately does **not** hook native
call sites (see the hook audit — it's a symmetric non-goal of the hook engine),
so an auto-maintained Vehicle iterator can't be built the same way. Two honest
options, to decide with the user:

1. **Tracked wrappers** — ship `veh_create(...)`/`veh_destroy(...)` that call the
   native and `setadd`/`setremove` a `Vehicle` set. Clean, native, portable, but
   changes the user's call sites (they call the wrapper, not `CreateVehicle`).
2. **Native-call interception in `dynhook`** — extend the companion to hook the
   AMX native dispatch (`amx_Callback`/native table) so `CreateVehicle` is
   intercepted transparently. Bigger, and the only route to true drop-in vehicle
   tracking; still no bytecode scan.

Players need neither (their lifecycle is callback-driven), which is why the
Player iterator ships now and the others wait on that decision.

## Decision: wrapper includes (shipped) — `<vehicles>`, `<actors>`

User chose the tracked-wrapper approach (not native interception). Shipped
`compiler/include/vehicles.inc` and `actors.inc`: thin `stock` wrappers that
add-to / remove-from a `Vehicle` / `Actor` set as they call the native.

```pawn
#include <vehicles>
new v = Vehicle_Create(model, x, y, z, a, c1, c2, respawn);
foreach (new id : Vehicle) { ... }
Vehicle_Destroy(v);
```

### Live proof (`veh_test.pwn`, real omp-server + iterset)

```
[V] created 1 2 3 len=3
[V] foreach:  1  2  3
[V] after destroy 2, len=2 has=0
  1  3
[DONE] veh
```

`Actor_Create`/`Actor_Destroy` are the same pattern (compile-verified).

**Documented limitation:** only entities created *through the wrapper* are
tracked — a raw `CreateVehicle` or another script's vehicles are not in the set.
True auto-tracking of every vehicle would need native-call interception in
`dynhook` (deferred; the wrapper is the portable, no-surprises choice).

## Files
`compiler/include/players.inc`, `vehicles.inc`, `actors.inc` (the iterators),
`players_test.pwn`, `veh_test.pwn` (live proofs).
