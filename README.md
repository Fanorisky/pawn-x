# pawn-x

**Native Pawn language features that replace YSI's script-side layer** — for the
[open.mp](https://github.com/openmultiplayer) / SA-MP Pawn compiler.

YSI implements varargs, iterators, and hooks with fragile script-side machinery
(macros, `#emit` assembly, and runtime bytecode scanning/rewriting). pawn-x moves
those into the compiler and a small companion plugin, so you get the same
abilities — **cheaper, deterministic, ~19× smaller output, and no bytecode
surgery** — as a standalone replacement (not a co-resident of YSI).

Two pillars, split by *when the information exists*:

- **Compiler** — anything known at compile time: `___` varargs, the `foreach`
  keyword + compact-set natives, `iterfunc` generators, the `hook` keyword, and
  leaner `switch` codegen (a `case a..b:` range compiles to one bounds-check
  instead of one table record per value). Pure codegen, no new opcodes; the
  `.amx` runs on any AMX host.
- **Companion plugin** (`dynhook`) — the runtime-only piece: add / remove /
  replace hook handlers while the server runs, and transparently intercept
  built-in callbacks. Inline-hooks `amx_Exec` via subhook — no open.mp SDK, so
  it stays SA-MP compatible too.

## Features vs YSI

| pawn-x | replaces | proven live |
|---|---|---|
| `___` varargs forwarding | `y_va` | exp 001, 005 |
| `foreach` + `set*` natives (+ multi-dim) | `y_iterate` / `y_foreach` | exp 002/003/005 |
| `iterfunc` generators | y_iterate custom iterators | exp 003 |
| `yield` coroutine generators | `#define Iterator@N iteryield` + `yield` | tests `yield_*` |
| `async`/`await` coroutines | `y_async` (*abandoned sketch — never shipped*) | tests `async_*` |
| `hook` keyword (+ `hook:N` priority) | `y_hooks` (compile-time) | exp 004/005 |
| `hook native`/`function`/`stock` + `continue` (call-site, incl. variadic `...` targets) | `y_hooks` real fn/native hooking | tests `chook_*` |
| `dynhook` runtime hooks | *(YSI has no runtime equivalent)* | exp 006/007 |
| compact `switch` codegen (range cases → bounds-check) | *(stock-Pawn table bloat)* | exp 013 |
| `inline` closures + `using inline`/`using public<sig>` + `Callback:` | `y_inline` | tests `inline_*` |

Most rows were run on a real `omp-server` and diffed against YSI (the `yield`,
`async`/`await`, and `hook native`/`function`/`stock` rows are proven by the
`yield_*` / `async_*` / `chook_*` compiler tests, not a live server run — and
`async`/`await` has no host adapter yet); see `experiments/*/RESULT.md`.

## Quickstart

```pawn
#include <pawn-x>          // foreach + set* + iterfunc + hook + dynhook

new players[MAX_PLAYERS];

MyLog(const fmt[], ...)    { printf(fmt, ___); }          // varargs forwarding

hook OnPlayerConnect(playerid)                            // stack multiple hooks
{
    setadd(players, playerid);
    return HOOK_CONTINUE;
}

main()
{
    foreach (new id : players) MyLog("online: %d", id);   // native iteration
}
```

Do **not** include YSI's `y_va` / `y_iterate` / `y_hooks` in the same script —
pawn-x reserves `foreach` and `hook` as keywords and the includes error out if
YSI is detected. Coming from YSI? See `docs/MIGRATION.md` for the full API mapping.

## Feature reference

**Varargs (`___`)** — forward a variadic tail into any function/native:
```pawn
Log(const fmt[], ...)              { printf(fmt, ___); }        // forward all
After(a, b, const fmt[], ...)      { printf(fmt, ___); }        // after fixed args
Build(dst[], size, const f[], ...) { format(dst, size, f, ___); }
Fmt(const fmt[], ...)              { new s[96]; format(s, sizeof s, fmt, ___); return s; }
```

**Iteration (`foreach` + compact-set natives)** — a plain array is a sorted set:
```pawn
new s[64];
setadd(s, 7); setremove(s, 3); sethas(s, 7); setlen(s);
setfree(s); setrandom(s); setget(s, 0); setalloc(s);
foreach (new v : s) { }                 // ascending
foreach (new v : Reverse(s)) { }        // descending; safe to setremove(v) inside
new grid[MAX_VEH][CAP];                 // multi-dimensional = plain 2D array
foreach (new p : grid[vid]) { }
```

**Generators (`iterfunc`)** — lazy, zero-alloc; `<iterators>` ships Range/RangeStep/Powers/Fib:
```pawn
iterfunc stock Count(cur, lo, hi) { if (cur==ITER_STOP) return lo<hi?lo:ITER_STOP; return cur+1<hi?cur+1:ITER_STOP; }
foreach (new i : Count(0, 10)) { }
iterfunc stock Fib(&acc, cur, lim) { ... }   // leading &ref = persistent state (Fibonacci etc.)
```

**Coroutine generators (`yield`)** — write the sequence straight-line; each `yield return` suspends and resumes on the next step (no `#define Iterator@N iteryield` needed, unlike YSI):
```pawn
iterfunc Count(n) { for (new i = 0; i != n; ++i) yield return i; }   // scalar locals only
foreach (new v : Count(3)) { }               // v = 0, 1, 2; `return;` ends the sequence
```

**Native `async`/`await` (`#include <async>`)** — write sequential code over callback-style ops; the function suspends at each `await` and resumes when the op completes. Single-threaded (a coroutine transform, not parallelism), single-`.amx`, linear bodies (MVP). Scalar **and** array/string/multi-dim *locals* survive an `await` — they are lifted into the coroutine's own state block, natively, no plugin. YSI only ever *sketched* `y_async` — pawn-x is the first to actually implement it:
```pawn
#include <async>

async GetScore(playerid)
{
    new base = 100;
    new tag[16] = "player";                   // array/string locals survive too
    new s = await AddScore(playerid, base);   // suspend; resumes with the result
    printf("%s: score=%d\n", tag, s + base);  // 'base' and 'tag' both intact
}

main()
{
    new t = Async_Start(GetScore, 7);         // starts; suspends at the await
    Async_Resume(t, AddScore(7, 100));        // a pump/host completion resumes it
}
```
`await asyncFn(args)` composes (the inner `return` resumes the awaiter). Completion is driven by `Async_Resume(token, value)` — a synthetic pump in tests, a thin timer/dialog/DB adapter on a live host (out of MVP scope). Combinators are native: `Async_All(n)`/`Async_Any(n)` + `await Async_Wait(g)` fan several operations into one awaiter (`task_all`/`task_any` parity), driven by the `Async_GateFeed` seam. Faults are native too: `Async_ResumeError`/`Async_GateFail` report failure, observed via `Async_Failed()`/`Async_Error()` after the await; `Async_Fail(err)` + `return` auto-raises up a composed `await asyncFn()` chain (`task_set_error` parity). A real host adapter ships: `async_omp.inc` gives `await Async_Ms(ms)` on open.mp `SetTimerEx` — validated on a live open.mp 1.5.8 server (real timers resume coroutines off the tick loop, arrays/scalars survive, combinators + faults work, arena returns to baseline; see `experiments/012-native-async/HOST-VALIDATION.md`). Supported across an `await`: scalar and array/string/multi-dim *locals*; **fixed-size array/string *parameters*** (copied into the coroutine block — `foo(buf[4])`); a *leaf* await in a `for`/`while`/`do` loop; **mid-expression** await, leaf or composed (`p + await F()`, `base + await Work()`); a **composed** `await asyncFn()` inside a loop; a leaf `await` as an argument to a **fixed-arity** call (`foo(await F(), p, q)`, any position); and **multiple awaits in one statement** (`await A() + await B()`). Multi-result delivery via `Async_ResumeArr`/`Async_InboxArr` (`await_arr` parity). Still compile-rejected (error): **unsized**/multi-dim/`&`reference *params* (268), `await` inside a `foreach` (099), a leaf `await` inside a **variadic** call's argument list (099 — no safe spill bound; hoist to a statement), a **composed-then-leaf** pair in one expression (099 — reorder or split), and a suspend at arbitrary call-stack depth in a non-async helper. See `docs/MIGRATION.md` for the `y_async` mapping and the PawnPlus comparison.

**Entity iterators** — ready-made connected-players / tracked-vehicle sets:
```pawn
#include <players>                          // Player: auto-tracked via connect/disconnect hooks
foreach (new id : Player) { }
#include <vehicles>                          // Vehicle: tracked via Vehicle_Create/Vehicle_Destroy
new v = Vehicle_Create(model, x,y,z, a, c1,c2, respawn);
foreach (new id : Vehicle) { }               // (<actors> is the same pattern)
```

**Hooks (`hook`)** — many handlers per callback, source order, optional priority + default return:
```pawn
hook OnFoo(a)     { ...; return HOOK_CONTINUE; }   // 1 run next / 0 = HOOK_CONTINUE_0
hook:100 OnFoo(a) { ...; return HOOK_STOP;     }   // higher priority first; -1 cancels, returns 0
hook default OnPlayerCommandText = 0;              // fall-through default (like YSI HOOK_RET)
```

**Call-site hooks (`hook native`/`function`/`stock` + `continue`)** — intercept every in-script call to a real native or pawn function/stock (single-`.amx`; fixed-arity or variadic `...` targets):
```pawn
hook function ComputeScore(p) { return continue(p) + 1; }  // continue = next hook, else the original
hook native random(range)     { return continue(range) % 8; }
hook function Sum(base, ...)  { return continue(base, ___); } // variadic: forward the tail with ___ (or bare continue())
// continue: 0×=replace, 1×=pass-through, N×=call original N times; args forwardable.
// In a variadic body numargs()/getarg(n)/setarg(n) use the user index; the hidden chain index is invisible.
// Forward references (a call before the hook) are redirected too.
```

**Runtime hooks (`dynhook`, needs the companion plugin)**:
```pawn
dynhook_intercept("OnPlayerDeath");                 // fire the chain automatically
dynhook_add("OnPlayerDeath", "MyHandler");          // add / remove / replace at runtime
dynhook_call("MyCustomEvent", "is", id, "hi");      // or dispatch a custom event
```

**Compact `switch` codegen** — stock Pawn expands a `case a..b:` range into one
`(value, address)` table record *per value*, so `case 0..9999:` alone becomes
10 000 records (a ~31 KB `.amx`). pawn-x coalesces each range into a single
inline bounds-check and only sends the leftover discrete values through the
`OP_SWITCH` table, so the same switch is **119 bytes** (≈267× smaller). Pure
codegen with existing opcodes, so the `.amx` still runs on any AMX host, and all
existing semantics (default, duplicate/overlap `error 040`, enum exhaustiveness)
are preserved:
```pawn
new state = Classify(x);
switch (state)
{
    case 0..9999:      Big();       // one bounds-check, not 10 000 table records
    case -1000..-1:    Negative();  // signed ranges work
    case 40000:        Lone();      // discrete values still use the lean table
    default:           Other();
}
```
Nothing changes in how you write `switch` — only the emitted code shrinks. See
`experiments/013-switch-codegen/RESULT.md` (measured + validated on a live
open.mp 1.5.8 server).

## Build

```sh
# 1. compiler (pawncc / pawnruns)
cmake -S compiler/source/compiler -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS="-m32"
cmake --build build -j$(nproc)

# 2. iterset plugin (set* natives on the server)
gcc -m32 -shared -fPIC -DLINUX -Icompiler/source/amx -Icompiler/source/linux \
  deps/iterset/iterset.c -o iterset.so

# 3. dynhook plugin (runtime hooks) — needs subhook (upstream Zeex/subhook is
#    gone; clone the Dasharo/subhook or tianocore/edk2-subhook mirror to deps/subhook)
gcc -m32 -fPIC -DSUBHOOK_STATIC -c deps/subhook/subhook.c -o /tmp/subhook.o
g++ -m32 -shared -fPIC -DLINUX -DSUBHOOK_STATIC -Icompiler/source/amx \
  -Icompiler/source/linux -Ideps/subhook \
  experiments/006-companion-plugin/dynhook.cpp /tmp/subhook.o -o dynhook.so
```

Compile a script: `build/pawncc gm.pwn -icompiler/include -i<stdlib>`.
Run the test suite: `tools/run-tests.sh -r build/pawnruns build`.
Deploy the `.so` plugins to the server's `plugins/` and list them under
`pawn.legacy_plugins` in `config.json` (open.mp) / `plugins` (SA-MP).

## Layout

| Path | Contents |
|---|---|
| `compiler/` | Vendored Pawn compiler (modified) + `compiler/include/` pawn-x includes |
| `experiments/` | Each feature: spec, tests, and a `RESULT.md` with live proof |
| `ysi-5/` | Vendored YSI 5 (reference for what is replaced) |
| `deps/`, `openmp/` | Build deps and server package (gitignored) |
| `docs/` | Coding standards, test baseline, design specs |

See `docs/CODING_STANDARDS.md` before contributing.

## License

Vendored compiler © ITB CompuPhase 1997-2006, community-modified (`compiler/license.txt`).
subhook © Zeex (BSD). YSI is MPL 1.1 — see `CREDITS.md`.
