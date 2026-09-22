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
  keyword + compact-set natives, `iterfunc` generators, and the `hook` keyword.
  Pure codegen, no new opcodes; the `.amx` runs on any AMX host.
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
| `hook` keyword (+ `hook:N` priority) | `y_hooks` (compile-time) | exp 004/005 |
| `dynhook` runtime hooks | *(YSI has no runtime equivalent)* | exp 006/007 |

Every row was run on a real `omp-server` and diffed against YSI; see
`experiments/*/RESULT.md`.

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

**Generators (`iterfunc`)** — lazy, zero-alloc; `<iterators>` ships Range/RangeStep/Powers:
```pawn
iterfunc stock Count(cur, lo, hi) { if (cur==ITER_STOP) return lo<hi?lo:ITER_STOP; return cur+1<hi?cur+1:ITER_STOP; }
foreach (new i : Count(0, 10)) { }
```

**Hooks (`hook`)** — many handlers per callback, source order, optional priority:
```pawn
hook OnFoo(a)     { ...; return HOOK_CONTINUE; }   // 1 run next / 0 = HOOK_CONTINUE_0
hook:100 OnFoo(a) { ...; return HOOK_STOP;     }   // higher priority first; -1 cancels, returns 0
```

**Runtime hooks (`dynhook`, needs the companion plugin)**:
```pawn
dynhook_intercept("OnPlayerDeath");                 // fire the chain automatically
dynhook_add("OnPlayerDeath", "MyHandler");          // add / remove / replace at runtime
dynhook_call("MyCustomEvent", "is", id, "hi");      // or dispatch a custom event
```

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
