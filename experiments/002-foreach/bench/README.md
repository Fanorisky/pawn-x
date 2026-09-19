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

## Add/contains benchmark (via legacy plugin)

The follow-up above is now measured. The `set*` natives are registered on the
real omp-server through a small open.mp **legacy plugin** (`iterset.so`), so
`setadd`/`sethas` run as compiled C against YSI's `Iter_Add`/`Iter_Contains`
(interpreted bytecode), same server, same workload.

### The plugin

`iterset.c` (kept alongside this README; built into `deps/iterset/iterset.so`,
gitignored) is a 32-bit `.so` implementing the open.mp legacy-plugin ABI
(`Supports` → `0x00010200`, `Load` grabs `amx_GetAddr`/`amx_Register` from the
exports table at `ppData[16]` indices 13/33, `AmxLoad` calls `amx_Register`).
The five natives are the compact-set logic copied **verbatim** from
`compiler/source/amx/itercore.c`, with each `amx_GetAddr` routed through the
pointer handed to `Load` (so the plugin links no amx.c). Build:
```
gcc -m32 -shared -fPIC -DLINUX \
  -Icompiler/source/amx -Icompiler/source/linux \
  deps/iterset/iterset.c -o deps/iterset/iterset.so
```
Deploy: copy to `openmp/Server/plugins/iterset.so`, set `config.json`
`pawn.legacy_plugins` to `["iterset"]`. Probe (`plugin_probe.pwn`, our pawncc +
`<foreach>`) confirmed it loads and resolves: `PROBE len=2 has7=1 has9=0`.

### Workload

Symmetric on both sides (`bench_native_ops.pwn` = our pawncc + `<foreach>`
natives; `bench_ysi_ops.pwn` = stock pawncc + YSI, same flags/patches as above):
- **add**: `ADDR=4000` rounds of `(reset + add 0..N-1)`, `N=400` →
  1,600,000 add ops. `setinit`+`setadd` vs `Iter_Clear`+`Iter_Add`.
- **has**: `HASR=4,000,000` membership queries, value cycling `0..N-1` (all
  present). `sethas` vs `Iter_Contains`.

### Results (min of 3, -d0)

| op (server) | native (C plugin) | YSI y_iterate |
|---|---|---|
| add — 1.6M inserts | **33 ms** | 2415 ms |
| has — 4M queries | **86 ms** | 93 ms |

### Honest conclusion

- **add**: native is ~73× faster (33 vs 2415 ms) — but read this with the
  workload in mind. Inserting `0..N-1` in ascending order is the **best case for
  the compact sorted set**: every `setadd` binary-searches to the end and
  appends with zero tail-shift (O(log n), no memmove). YSI's index-set keeps a
  sorted linked list, and ascending inserts are not similarly free for it. The
  C-vs-bytecode gap is real and large, but this particular ordering flatters the
  compact set; a randomized insertion order (which forces tail-shifts on the
  native side) would narrow it. The direction (native wins add) is robust; the
  73× magnitude is workload-specific.
- **has**: essentially a **tie** (86 vs 93 ms). This is the surprise. Native
  `sethas` is compiled C but O(log n) binary search *and* pays the AMX
  native-call boundary (param marshalling) on every one of the 4M calls
  (~21 ns/call). YSI `Iter_Contains` on an index-set is O(1) (the value *is* the
  slot) and inlines as bytecode with no call boundary. The C speed advantage and
  the native-call overhead roughly cancel, so "compiled C beats interpreted
  Pawn" does **not** hold for membership here — the data model and the call
  boundary matter more than C-vs-bytecode.

Net: native wins the mutation-heavy `add` decisively (with the ordering caveat),
and membership is a wash. Combined with the iteration result above (native walk
~1.28× slower than YSI), neither side dominates across the board — each data
model wins the operation that suits its shape.
