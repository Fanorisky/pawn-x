# foreach benchmark — native vs YSI (real open.mp server)

A like-for-like iteration benchmark of pawn-x native `foreach` against YSI
`y_iterate`, both run on the **real omp-server** (v1.5.8.3079), timed with
`GetTickCount()` around the loop only.

## Workload

Sum a 400-element set, 200,000 times = **80,000,000 iterations**. Both sides
compute the identical accumulator, confirming they do the same work.

- `bench_native.pwn` — a global array used as a compact set (`data[0]`=count,
  `data[1..count]`=values), walked by the native `foreach` keyword. The set is
  hand-filled (values 0..399) so the walk needs no `set*` natives (those aren't
  registered on the stock server; the walk itself is pure AMX opcodes).
- `bench_ysi.pwn` — a real `Iterator:gset<400>`, `Iter_Add`, and YSI `foreach`.

## Toolchains

Two compilers are needed because our modified `pawncc` reserves `foreach` as a
keyword, which collides with YSI's `foreach` macro:

- **Native side** — our modified `build/pawncc` (has the `foreach` keyword):
  ```
  build/pawncc -d0 -i openmp/Server/qawno/include \
    openmp/Server/gamemodes/bench_native.pwn -o .../bench_native
  ```
- **YSI side** — a pristine stock `pawncc` built from the initial vendored
  commit (no pawn-x features, `sNAMEMAX`=31, which YSI requires):
  ```
  git worktree add /tmp/pawn-stock 461814d
  cmake -S compiler/source/compiler -B /tmp/pawn-stock/build-stock \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS="-m32" && cmake --build ...
  /tmp/pawn-stock/build-stock/pawncc -d0 -Z+ \
    -i openmp/Server/qawno/include -i deps/amx_assembly -i ysi-5 \
    openmp/Server/gamemodes/bench_ysi.pwn -o .../bench_ysi
  ```
  Flags: `-Z+` (SA-MP compat: backslash includes), `-d0` (no debug `break`
  opcodes — required for a fair timing; a debug build inflates both sides
  unevenly). `#define AMX_OLD_CALL` and the `FOREACH_NO_*` defines are in the
  `.pwn`.

  Deps (fetched): `deps/amx_assembly` (pawn-lang/amx_assembly),
  `deps/samp-stdlib` (unused once open.mp includes are used).

  **YSI patches required to compile against current open.mp stdlib** (this
  YSI 5 predates them; both are peripheral to the iterator being measured):
  - `YSI_Core/y_utils.inc` — `stock ftouch` guarded out (`#if 0`); open.mp
    stdlib now ships a native `ftouch` (collision, error 021).
  - `YSI_Data/y_foreach/iterators.inc` — the built-in custom-iterator
    generators (`Range`/`Powers`/`Fib`/`Random`/`Null`/`NonNull`/`Until`/
    `Filter`) wrapped in `#if 0`; they mis-parse under this toolchain
    (`iterfunc ... [cellmin]` → error 009) and are unused by the benchmark.
  These patches are NOT committed to the vendored `ysi-5/` (kept pristine);
  re-apply them to rebuild `bench_ysi`.

## Running

Set `config.json` `pawn.main_scripts` to `["bench_native 1"]` (or
`["bench_ysi 1"]`), run `timeout -s KILL 12 ./omp-server`, grep the log for the
`BENCH ...` line, repeat 3× and take the min (single runs are noisy ±~10%;
`GetTickCount` brackets only the loop, so startup is excluded). Restore
`main_scripts` to `["gungame 1"]` afterward.

## Results (min of 3, -d0)

| walk (80M iterations) | time |
|---|---|
| YSI `foreach` (index-set linked list) | **404 ms** |
| native `foreach` — before optimization (indexed `lidx`) | 679 ms |
| native `foreach` — after pointer-walk optimization | **518 ms** |

Optimization (`perf: foreach walks a pointer`): the walk was rewritten from an
indexed load (`lidx` = index*cell+load, plus base/index reloads each iteration)
to a **pointer walk** — `p` runs from `&array[1]` to `&array[count+1]`, and each
iteration is `load.s.pri p / load.s.alt pend / jsgeq exit / load.i / stor`, then
`p += cell / jump`. No `lidx`, no base reload, and `jsgeq` fuses compare+branch
while leaving `p` in PRI for the immediate `load.i`. Result: **679 → 518 ms
(~1.24× faster)**.

## Honest conclusion

Native `foreach` is now **~1.28× slower than YSI** on this walk (518 vs 404 ms),
down from ~1.68×. The residual gap has two causes:

1. **Architectural (inherent):** our compact set is a **value-set** — each step
   needs `value = *p` (one indirection). YSI is an **index-set** where the value
   *is* the position, so its walk (`cur = next[cur]`) has no separate value
   fetch. This is the flip side of the value-set design (cheap memory for sparse
   / arbitrary-magnitude values; see RESULT.md), and it is not removable without
   changing the data model.
2. **Runtime constraint:** the loop body clobbers PRI/ALT, so `p`/`pend` must
   round-trip through stack cells each iteration (true for YSI too, but YSI does
   fewer such round-trips per step). Closing this further needs register-resident
   walk state or a dedicated iteration opcode — out of scope here.

Where native is expected to win (unmeasured on the server — the `set*` natives
aren't registered there): `setadd`/`sethas` run as compiled C, versus YSI's
`Iter_Add`/membership in interpreted Pawn bytecode. Measuring that fairly on the
server would need shipping `set*` as an open.mp component/legacy plugin.
