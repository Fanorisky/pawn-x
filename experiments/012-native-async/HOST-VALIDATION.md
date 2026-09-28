# Native async: REAL open.mp host validation (exp 012 phase 3)

**Date:** 2026-09-25  **Branch:** `feat/native-async`

## Question
Does the native async stack (previously driven only by a synthetic pump under
`pawnruns`) run on a real open.mp server, resumed by real timers, with a native
host adapter and no plugin? (The MVP had listed "a real host adapter" as a
non-goal / open question.)

## Answer: YES, fully validated.

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
>>> started; 9 coroutines parked on REAL timers; control returned
[async] loop: iteration 1 done, sum=1
[async] midexpr: a+b survived the timer, r=42
[async] countdown: tick @200ms
[async] loop: iteration 2 done, sum=3
[async] fault: caught err=404 (recovered)
[async] auto-raise: outer caught err=902
[async] array: buf survived the timer, sum=140
[async] combinator: both timers fired, total=33
[async] loop: iteration 3 done, sum=6
[async] loop: all iterations done, sum=6
[async] countdown: tick @400ms (base still 1000)
[async] countdown: DONE
[async] lifetime: active coroutines now = 0 (expect 0)
```

## What this proves on a live host (not a synthetic pump)
- **Suspend/return/resume** across a real `SetTimerEx` timer: `OnGameModeInit`
  starts the coroutines, they park on real timers, control returns, the tick loop
  resumes each in place.
- **Loop-carried await**: `await Async_Ms(120)` inside a `for` loop re-arms a real
  timer each iteration (a periodic async task, `sum=6` over 3 iterations).
- **Mid-expression await**: `a + b + await Async_Ms(160)`. The operand temporaries
  `a`,`b` are spilled into B and survive the real-timer suspend (`r=42`).
- **Scalar locals** (`base=1000`) survive across two real-timer awaits.
- **Array locals** (`buf[8]`) lifted into the coroutine block survive a real-timer
  await (`sum=140`).
- **Combinators** (`Async_All`) fed by two real timers resume with the aggregate
  (`total=33`).
- **Fault channel** (`Async_ResumeError` from a timer) is observed via
  `Async_Failed()`/`Async_Error()` (`err=404`).
- **Fault AUTO-RAISE** (`Async_Fail`) propagates a real-timer-driven inner's fault
  up a composed `await` into its awaiter (`err=902`).
- **Lifetime**: the arena returns to 0 after all coroutines complete, no leak
  on the real host, even with the re-arming loop.
- **No runtime errors**, no plugin: only the patched compiler's emitted code plus
  the script-side `async.inc` + `async_omp.inc`.

## Net
The "no host adapter (MVP non-goal)" gap vs PawnPlus is closed natively: real
timer/callback awaitables work on a live open.mp server, leaner than PawnPlus (only
lifted locals live in the coroutine block; no whole stack+heap snapshot, no plugin).

---

## Coexistence + capability-gap demo (2026-09-26)

`comparison/combo_gap.pwn` runs the pawn-x NATIVE coroutine and PawnPlus in one
`.amx`, compiled by `build/pawncc`, and includes a case PawnPlus supports but
pawn-x native does not: two suspends in one expression. Native lifts only the
coroutine's own locals, so it allows at most one `await` per statement; PawnPlus
snapshots the whole frame, so `task_await(a) + task_await(b)` works. Setup:
`legacy_plugins: ["PawnPlus"]`, `main_scripts: ["combo_gap 1"]`.

Real server output (open.mp 1.5.8 + PawnPlus 1.5.3, one `.amx`):

```
 PawnPlus v1.5.3 loaded
>>> ONE .amx (build/pawncc): pawn-x native async + PawnPlus, plus a PawnPlus-only case
[native]   start (pawn-x coroutine, no plugin)
[pawnplus] TWO suspends in ONE expression: r=42  (pawn-x native: error 099)
[native]   local survived TWO awaits (separate statements): acc=42
```

Both reach 42: the native coroutine carries a lifted local across two
separate-statement awaits; PawnPlus carries the first await's result across the
second suspend within one expression. The native equivalent of the latter is a
compile error: `comparison/gap_native_reject.pwn` (`new r = await Async_Ms(150) +
await Async_Ms(150);`) is rejected by `build/pawncc`:

```
gap_native_reject.pwn(12) : error 099: a "yield" cannot appear where stack storage is live ...
```

## Net (gap)
Native and PawnPlus coexist in one runtime and one `.amx`. The remaining
capability edge is real and documented: PawnPlus's frame-snapshot suspends at
arbitrary expression positions (more than one `await` per statement, `await` inside
a variadic call), which pawn-x native rejects cleanly (error 099) rather than
miscompile. Split into separate statements is the native idiom.
