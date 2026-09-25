# Native async — REAL open.mp host validation (exp 012 phase 3)

**Date:** 2026-09-25  **Branch:** `feat/native-async`

## Question
Does the native async stack — previously driven only by a synthetic pump under
`pawnruns` — run on a **real open.mp server**, resumed by **real timers**, with a
**native host adapter and no plugin**? (The MVP had listed "a real host adapter"
as a non-goal / open question.)

## Answer: YES — fully validated.

`compiler/include/async_omp.inc` is the host adapter: it bridges the
`Async_Resume(token,value)` seam to open.mp's `SetTimerEx`, giving real awaitables
(`Async_Ms`/`Async_Ticks`) plus `Async_Self()` (a coroutine's own stable token, so
re-arming across many awaits never leaks an alias slot). A callback awaitable uses
the same seam: a `public` callback calls `Async_Resume`/`Async_ResumeError`/
`Async_GateFeed`.

## Reproduce
```
# compile the demo gamemode with the patched compiler
build/pawncc openmp/Server/gamemodes/async_host.pwn \
    -iopenmp/Server/qawno/include -icompiler/include \
    -oopenmp/Server/gamemodes/async_host.amx
# point the server at it and run (headless)
#   config.json: "pawn": { "main_scripts": ["async_host 1"] }
cd openmp/Server && ./omp-server        # Ctrl-C after ~1s
```

## Observed server log (open.mp 1.5.8, Timers.so)
```
>>> async host-adapter test: starting coroutines
[async] countdown: start
>>> started; 7 coroutines parked on REAL timers; control returned
[async] countdown: tick @200ms
[async] fault: caught err=404 (recovered)
[async] auto-raise: outer caught err=902
[async] array: buf survived the timer, sum=140
[async] combinator: both timers fired, total=33
[async] countdown: tick @400ms (base still 1000)
[async] countdown: DONE
[async] lifetime: active coroutines now = 0 (expect 0)
```

## What this proves on a live host (not a synthetic pump)
- **Suspend/return/resume** across a real `SetTimerEx` timer: `OnGameModeInit`
  starts the coroutines, they park on real timers, control returns, the tick loop
  resumes each in place.
- **Correct chronological interleaving** of independent coroutines by timer delay
  (200 → 250 → 280 → 300 → 350 → 400 ms), all from one tick loop.
- **Scalar locals** (`base=1000`) survive across two real-timer awaits.
- **Array locals** (`buf[8]`) lifted into the coroutine block survive a real-timer
  await (`sum=140`).
- **Combinators** (`Async_All`) fed by two real timers resume with the aggregate
  (`total=33`).
- **Fault channel** (`Async_ResumeError` from a timer) is observed via
  `Async_Failed()`/`Async_Error()` (`err=404`).
- **Fault AUTO-RAISE** (`Async_Fail`) propagates a real-timer-driven inner's fault
  up a composed `await` into its awaiter (`err=902`).
- **Lifetime**: the arena returns to **0** after all coroutines complete — no leak
  on the real host.
- **No runtime errors**, no plugin — only the patched compiler's emitted code plus
  the script-side `async.inc` + `async_omp.inc`.

## Net
The "no host adapter (MVP non-goal)" gap vs PawnPlus is **closed natively**: real
timer/callback awaitables work on a live open.mp server, leaner than PawnPlus (only
lifted locals live in the coroutine block; no whole stack+heap snapshot, no plugin).
