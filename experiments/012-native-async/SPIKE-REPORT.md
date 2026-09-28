# Native `async`/`await` de-risk spike: report (exp 012)

**Branch:** `feat/native-async`  **Date:** 2026-09-25
**Scope:** investigation + minimal runnable spike to prove the compiler can emit an async
function that suspends at `await`, returns to its caller, and is resumed later by an
in-script scheduler carrying the awaited value.

---

## 1. VERDICT

**Native `async`/`await` is FEASIBLE via Approach 1 (reuse the `yield` coroutine
engine, swap the `foreach` driver for a scheduler). Proven with a runnable spike.**

The open question the spike had to answer (*can a `yield`-style generator be resumed by
something other than the `foreach` driver, from a foreign call context, and can a value be
delivered INTO the body on resume?*) is answered YES on all counts. No new opcodes and
no VM changes were needed; the fallback CPS transform (Approach 2) is not required.

## 2. The concrete mechanism that worked

A `yield` generator's resume is already just a plain `call Gen(B, args)`: the suspend
(`@yield.emit`) fully unwinds the generator's stack frame back to its caller, leaving ALL
state in the heap block `B`. So resuming from a *different* frame (a scheduler/pump) is
structurally identical to `foreach`'s second loop iteration; there is nothing tying resume
to the original caller. The only thing `yield` lacks for `await` is a way to deliver a value
*into* the body on resume (yield is body→driver only); the spike adds a per-block **inbox
slot** for that.

### Compiler pieces added (all on `feat/native-async`)
- **Keywords** `async`, `await`, and two spike-only scheduler intrinsics `__async_start`,
  `__async_resume`: `compiler/source/compiler/sc.h` (token enum) +
  `compiler/source/compiler/scvars.c` (`sc_tokens[]`, matching index).
- **`uASYNC` usage flag** (`0x80000`) at `sc.h:310`. Set at declaration in `newfunc()`
  (`sc1.c:5636`) via `pc_async` (`sc1.c` decl switch `case tASYNC`, ~`sc1.c:2013`).
- **`generator_isgen()`** returns TRUE for `uASYNC` so the coroutine prologue is emitted
  pass-stably at `sc1.c:3181`.
- **`gen_reserved(sym)`** (`sc1.c:3199`) reserves the head cells of `B`: `1` for `yield`
  (`B[0]`=resume CIP), `2` for `async` (`B[0]`=CIP, `B[1]`=await-result inbox,
  `ASYNC_INBOX_SLOT`). Lifted param/local slots shifted by this in the prologue
  (`sc1.c:~3345`) and in `declloc()` (`sc1.c:~4075`).
- **`doawait()`** (`sc1.c:3499`) evaluates the awaitable (leaves a token in PRI), suspends
  through the existing `@yield.emit` helper (stores resume CIP in `B[0]`, unwinds frame,
  returns the token to the caller), then on resume emits `PRI = *(B + inbox)` so the
  scheduler-delivered value becomes the value of the `await` expression. Wired into the
  expression parser at `primary()` in `sc3.c:~2215`. Reuses yield's `error(99)` guard for
  live stack storage across a suspend, and errors if used outside an `async` function.
- **`doasyncstart()`** (`sc1.c:~3585`) is the scheduler START glue: `modheap` a zeroed `B`
  (size `gen_reserved+genlocals`), store `B` into a user handle var, and do the FRESH call
  `Gen(B, args...)`. Runs the body to the first `await`, which suspends back here; `B`
  survives on the heap. The async analogue of `doforeach`'s block-alloc + first call, but a
  single caller-owned call, not a loop. Wired at `statement()` `case t__ASYNCSTART`.
- **`doasyncresume()`** (`sc1.c:~3690`) is the scheduler RESUME glue: writes `value` into
  `B[inbox]`, then `call Gen(B)` from the pump's (foreign) frame; the prologue sees
  `B[0]!=0` and `sctrl 6` jumps straight back after the suspended `await`. Wired at
  `statement()` `case t__ASYNCRESUME`.

### Verified state-block layout (from `pawndisasm` of the spike)
```
B[0] = resume CIP (0 = fresh)          set by @yield.emit on suspend, read by prologue sctrl 6
B[1] = await inbox                     scheduler writes it before resume; doawait reads it
B[2..] = lifted params then locals     survive every suspend (base, s, etc.)
```
Prologue (`GetScore`): `stack -4; load.s.pri c(=B); stor.s.pri -4; load.i; jzer fresh; sctrl 6`.
Suspend: `... push token; push B; push.c 8; call .@yield.emit` → resume point is the next insn.
Resume read: `load.s.pri -4(=B); add.c 4; load.i` → PRI = `B[1]` = awaited value.

## 3. Runnable proof

Files (committed under `experiments/012-native-async/`), built with the patched `pawncc`,
run under `build/pawnruns`:

**`async_basic.pwn`**, one `await`, resumed by a synthetic pump from `main`→`Pump()`:
```
(started; suspended at await, control returned to caller)
score=207
```
`base=100` was set before the `await` and survived it; `s = 7+100 = 107`; `107+100 = 207`.
The "started/suspended" line prints BEFORE the resume: control genuinely returned to the
caller and the coroutine resumed later from `Pump()`'s frame. (Matches the design's `score=207`.)

**`async_two_awaits.pwn`**, three sequential `await`s in one body, pump-driven loop:
```
chain: a=6 b=16 c=116 final=143
```
`a=5+1`, `b=6+10`, `c=16+100`, `final=a+b+c+x=143`. All locals survive all three suspends,
resumed in order.

**`async_pending_multiple.pwn`**, two independent coroutines parked, resumed in REVERSE
order with distinct values:
```
(two workers parked)
worker id=2 local=9000 got=222 sum=9224
worker id=1 local=7000 got=111 sum=7112
```
Per-block state is isolated; arbitrary-order resume by handle works.

**Compile-fail**, `await` outside an `async` function:
```
error 255: "await" is only valid inside an "async" function
```

**Regression:** full suite is **193 PASSED, 2 FAILED**, identical to the clean-tree baseline
(the 2 = pre-existing `__timestamp`, `gh_353_symbol_suggestions`; verified by stashing the
patch, rebuilding, and re-running: same 193/2). Explicit `return <v>` in an async body also
compiles and runs (value not yet wired to an awaiter; that's the v2 return-to-awaiter piece).

## 4. Biggest remaining risks / unknowns for a full MVP

1. **Generic scheduler dispatch (resume ANY parked coroutine by token).** The spike's
   `__async_resume` names the function so resume is a static `call`. A real scheduler holds
   many parked coroutines of different functions and must resume by token. The mechanism
   exists and is proven-available: store the coroutine's entry address in a `B` slot and use
   the **`call.pri`** opcode (indirect call; `amx.c:2295`, `__addressof` already yields a
   function's code address as a constant). Needs a `B[genaddr]` slot (bump `gen_reserved` to
   3) and a token→B registry. Low technical risk, but it's the main "productionization" gap.
2. **Fold START into the normal call path.** MVP wants `new t = Foo(7)` to start the
   coroutine (not an explicit `__async_start` intrinsic). That means intercepting `async`
   callees in `callfunction()` (sc3.c), the hot path the whole suite depends on, to
   allocate `B`, park, and yield a handle/token. Non-trivial and needs care to stay
   suite-green.
3. **Context-block lifetime / heap accounting.** `doasyncstart` does a bare `modheap` that
   is never released in the spike (leak by design). A real impl must own `B`'s lifetime in
   the scheduler and free it on completion, without the AMX heap-cursor model (LIFO `heap`)
   fighting a coroutine that outlives the frame that allocated it. Likely needs a companion
   allocator or a fixed registry arena rather than raw `modheap`.
4. **Return-to-awaiter chaining.** An `async` fn awaited by another `async` fn: the inner
   `return` must resume the outer. A generator's `return` currently ends the sequence
   (`ITER_STOP`); async needs `return <v>` to write a result slot + notify the awaiter via
   the scheduler. Design'd but untested.
5. **`await` in expression/control contexts beyond the linear MVP.** Inherits yield's v1
   limits (error 99/096): no live stack storage across a suspend, so `await` inside a
   `foreach`, as a function argument mid-push, in a compound sub-expression with live
   temporaries, or with array/string locals live across it, must be rejected or lifted.
   `await` as a bare RHS works; richer positions need the local-lifting envelope widened.
6. **JIT / host compatibility.** Same open question as `yield`: relies on `lctrl 6`/`sctrl 6`
   (CIP get/set) and `sctrl 4` (STK). Proven under `pawnruns`; unverified under the AMX JIT
   and on a live open.mp host (no host event loop in `pawnruns`, so real timer/dialog/DB
   adapters are untested).

## 5. Recommended task breakdown for the MVP

1. **T1 (DONE, this spike):** prove foreign-frame resume + value-in via the yield engine.
2. **T2, generic resume:** add `B[genaddr]` slot, emit the entry address at start, resume
   via `call.pri`; add a minimal token→B registry (in-script first, then companion-plugin).
   Test `async_pending_multiple` with mixed coroutine types.
3. **T3, natural call/await surface:** intercept `async` callees in `callfunction()` so
   `new t = Foo(args)` starts a coroutine and yields a handle/token; drop the
   `__async_start`/`__async_resume` intrinsics (keep them behind a debug flag if useful).
   Keep the suite green at each step.
4. **T4, return-to-awaiter:** add a result slot + completion notify; make an `async` fn
   awaitable by another; test `async_return_to_awaiter`.
5. **T5, lifetime & safety:** scheduler-owned `B` lifetime, free-on-complete, no leak/
   double-free (`async_pending_multiple` accounting); reentrancy guard for `Async_Resume`
   called mid-body.
6. **T6, guards & diagnostics:** dedicated error numbers (await-outside-async, array across
   await, await under live stack storage, `async` on a non-function); widen local-lifting so
   more `await` positions are legal or cleanly rejected.
7. **T7, host adapter + JIT:** one real adapter example (timer or dialog) bridging a host
   callback to the resume entry; verify under the JIT.

## Reproduce
```
cd /home/amba/pawn-lab
cmake -S compiler/source/compiler -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS="-m32"
cmake --build build -j$(nproc)
build/pawncc experiments/012-native-async/async_basic.pwn -i$(pwd)/compiler/include -oasync_basic.amx
build/pawnruns async_basic.amx
tools/run-tests.sh -r build/pawnruns build   # 193 PASSED, 2 FAILED (pre-existing)
```
