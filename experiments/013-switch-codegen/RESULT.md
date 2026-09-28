# 013 Switch codegen: ranges as inline bounds-checks (no table bloat)

## The long-standing oddity

Stock Pawn compiles `switch`/`case` into an `OP_SWITCH` + `casetbl` table of
`(value, address)` records. A **range** clause (`case a..b:`) is expanded
one record per value (`sc1.c` `doswitch`, the `while (++val<=end)` loop):
`case 0..9999:` becomes 10 000 table records (all pointing at the *same*
body) instead of a single bounds-check. That is the classic "switch bikin
.amx bengkak".

(The runtime is fine: this AMX build's `OP_SWITCH` already does a binary
search, `amx.c`: the problem is purely output size + load-time
relocation of every record.)

## Measurement (spike)

Compiled with the patched `build/pawncc`, `.amx` sizes:

| program | switch | before | after |
|---|---|---|---|
| `range_sw.pwn` | `case 0..9999:` | **31 856 B** | **119 B** (≈267× smaller) |
| `small_sw.pwn` | 2 discrete cases | 121 B | 124 B (+3 B) |
| `ifchain.pwn` | `if (x>=0 && x<=9999)` | 120 B | n/a (reference) |

`-a` listing confirmed the old output was **10 001 case records** for the one
range; the new output is a single inline bounds-check. The +3 B on the plain
discrete switch is one extra `jump` into the dispatch block, negligible.

## Mechanism (pure codegen, stock-AMX compatible)

The parse/overlap/enum-exhaustiveness logic is untouched (so `error 040`
duplicate detection across ranges still fires). Only the **emit** step
changed:

1. After the switch expression, `jump` over the case bodies to a dispatch
   block (the value stays in `PRI` across the jump).
2. The sorted, fully-expanded case list is coalesced into maximal runs of
   consecutive values sharing one body label. A run wider than one value is a
   range → one inline bounds-check:
   `const.alt lo; jsless skip; const.alt hi; jsgrtr skip; jump body`
   (signed compares, `PRI` preserved).
3. The leftover single values still go through the lean, binary-searchable
   `OP_SWITCH` table (or, if there are none, a direct `jump` to default).

No new opcode, no interpreter change: the `.amx` runs on any AMX host.

## Correctness

Compiler tests (`tools/run-tests.sh`): `switch_range` (runtime: ranges,
boundaries, gaps, negatives, default), `switch_range_overlap` (range↔discrete
overlap still `error 040`), `gh_96` (pcode layout). Full suite **194 passed /
2 failed**: the 2 are the pre-existing baseline failures (`__timestamp`,
`gh_353_symbol_suggestions`); zero regressions.

## Host validation (live open.mp 1.5.8)

`switch_host.pwn` (a 10 000-value range + a 1 000-value range + discrete
dispatcher) compiles to **1 042 B** and self-verifies every probe on a real
server:

```
[Info] SWITCH-HOST: PASS all probes classified correctly
```

Build: `build/pawncc gamemodes/switch_host.pwn -iqawno/include -o…`.
