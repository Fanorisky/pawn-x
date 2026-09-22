# Experiment 005: Parity Comparison — real YSI vs pawn-x native, on the live server

**Date:** 2026-09-22
**Server:** open.mp `omp-server` (real runtime, headless, killed after N seconds)
**Goal:** run two gamemodes side-by-side that exercise *every* ability of
`y_va`, `y_iterate`/`y_foreach`, and `y_hooks`, and find any case the native
features cannot replicate.

- `compare_native.pwn` — MODIFIED `pawncc`; uses `___`, `set*` natives +
  `set_foreach`, `iterfunc`, `hook`. Needs the `iterset` legacy plugin.
- `compare_ysi.pwn` — STOCK `pawncc` + YSI 5; uses the real YSI API.

Both print the *same labeled lines*; the two logs were diffed.

## How to reproduce

```
# native (our compiler + iterset plugin)
build/pawncc <gm>/compare_native.pwn -d0 -Z+ \
  -iopenmp/Server/qawno/include -icompiler/include -o <gm>/compare_native
#   config.json: main_scripts ["compare_native 1"], legacy_plugins ["iterset"]

# ysi (stock compiler built from vendored commit 461814d, YSI 5)
/tmp/pawn-stock/build-stock/pawncc <gm>/compare_ysi.pwn -d0 -Z+ \
  -iopenmp/Server/qawno/include -ideps/amx_assembly -iysi-5 -o <gm>/compare_ysi
#   config.json: main_scripts ["compare_ysi 1"], legacy_plugins []
```

**Toolchain patches the YSI side needs** (native side needs none):
- `#define AMX_OLD_CALL` (amx_assembly/YSI call-convention match).
- `#define PAWN_X_NO_NATIVE_FTOUCH` — a guard added to
  `openmp/Server/qawno/include/file.inc` so open.mp's native `ftouch` is
  suppressed and YSI's own `stock ftouch` compiles (the collision is error 021;
  guard is inert unless the define is set, so other gamemodes are unaffected).
- `#define PAWN_X_NO_YSI_ITERATORS` — a guard added to `ysi-5/YSI_Data/y_iterate.inc`
  because YSI's bundled generators (`y_foreach/iterators.inc`) **fail to compile**
  under this toolchain (error 009). Re-apply to rebuild; `ysi-5` is kept pristine.
- A `#include <YSI_Internal\y_unique>` between **every** same-name `hook` body.

## Result: outputs are identical except for four lines

43 labeled lines; **39 identical**. The differences:

| line | native | ysi | meaning |
|---|---|---|---|
| SET add | `1 0` (added / dup) | `30 -2147483648` (value / cellmin) | different **return convention**, not a capability gap — both track membership (len=3 both) |
| SET get | `get0 10 get2 30` | `-2 -2` (sentinel) | **native-only**: positional `setget`; YSI has no scalar `Iter_Get` |
| runtime-replace | `UNSUPPORTED` | `SUPPORTED` | **YSI-only**: `DEFINE_HOOK_REPLACEMENT` (runtime) |
| saferemove | `keep 10 / keep 30 / len 2 / [DONE]` | `keep 10` then **infinite loop** | **native-only**: remove-current is safe; YSI hangs |

Everything else — VA forwarding (all/after-fixed/nested), `format`, format+return,
membership/count/free/alloc/random, forward + reverse + break + continue walks,
multi-dimensional `grid[k]`, hook chaining with STOP control (`A`,`B` run, `C`
suppressed), and two hooks on the real `OnGameModeInit` callback — is
**byte-identical** between the two.

## The one case native genuinely cannot replicate

**Runtime hook add/remove/replace** (`DEFINE_HOOK_REPLACEMENT`, and manipulating
the chain at runtime). The native `hook` chain is fixed at compile time. This is
the *only* real capability gap, and it is runtime-only by nature → it belongs to
the **companion plugin** pillar, not the compiler. Everything a compiler can own
is covered.

## Cases where the native is *better* (found while probing)

1. **Safe removal of the currently-iterated element.** `set_foreach` +
   `setremove(current)` printed `keep 10 / keep 30`, ended `len 2`, and the
   script finished. The identical YSI pattern (`foreach` + `Iter_Remove(current)`)
   **hangs in an infinite loop** on this YSI 5 + open.mp + amx_assembly stack —
   reproduced in isolation (`ysi_saferemove` probe: prints `keep 10`, then the
   server has to be killed). This alone can lock up a production server.
2. **Real lazy generators.** Native `iterfunc` (`Range`) compiles and runs;
   YSI's bundled `iterators.inc` generators don't compile here at all (error 009),
   so the YSI side had to fake the range with a plain `for`.
3. **No per-hook boilerplate.** Native stacks `hook Name(){}` bodies directly;
   YSI needs a `#include <YSI_Internal\y_unique>` between each, or the second body
   is `error 021: symbol already defined`.
4. **Hooks are callable ex nihilo.** `hook NX_Chain(a)` makes `NX_Chain(99)`
   directly callable even though nothing declared it; YSI's `hook` only
   *intercepts* an existing public, so the target must be `forward`ed and invoked
   via `CallLocalFunction`.
5. **AMX size: 3.8 KB (native) vs 72.8 KB (YSI)** — ~19× smaller for the same
   abilities.
6. **One compiler, zero patches** vs a pinned stock compiler + four
   source/stdlib patches.

## Architectural note (not a gap, a trade-off)

Native is a **value-set** (sorted distinct values, arbitrary magnitude within the
array); YSI is an **index-set** (value == slot, bounded by `<cap>`). For ids in
`[0, cap)` — the common case — they are behaviourally identical (proven above).
Native can hold sparse large values YSI cannot; YSI's dense linked walk is
~1.28× faster (see exp 002 benchmark). Different tool, same job for id sets.

## Files
`compare_native.pwn`, `compare_ysi.pwn` (this directory). Raw captured logs were
diffed to 4 differing lines out of 43.
