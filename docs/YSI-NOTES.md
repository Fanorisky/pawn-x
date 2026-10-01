# Response to Y-Less's review (issue #1)

Alex "Y-Less" Cole (YSI's author) reviewed pawn-x in issue #1. This file tracks
each note: what was **fixed**, and what is a **deliberate difference** (with the
upgrade path he asked for). pawn-x's north star is a *native, cleaner* take on
YSI's script layer, not a byte-for-byte drop-in, so some differences are on
purpose, and this documents them so migration is predictable.

## Fixed

| Note | Was | Now |
|---|---|---|
| #1 `foreach` remove-mid-loop skipped/duplicated (`1 2 4 5 5`) | position walk over a compacting array | **value-based walk** (`setnext`/`setprev`): removing the current or a future element mid-loop is safe (`1 2 3 4 5`). Live-validated on open.mp. |
| #6 the user's own `public X` wasn't called when `X` was also hooked | dispatcher orphaned the body | the user `public` is **chained as the last link**, after every hook (`uHOOKORIG`). |
| #4 (const) `setadd`/`setremove`/`setinit`/`setalloc` marked `const` | wrong, they mutate | `const` dropped; read-only `sethas`/`setlen`/`setget`/`setfree`/`setrandom` keep it. |
| #13 side-note: empty state `<>` could crash (NULL deref, `assert` gone in release) | latent segfault | guarded: a malformed/empty state list is a clean error, never a crash. |
| #7 no way to detect the compiler | only `#tryinclude` | **`__PawnX`** builtin constant (`#if defined __PawnX`), independent of includes. |
| #2/#10 no migration shims | (none) | optional **`<ysi_compat>`**: `Iterator:name<N>` → `name[N+1]` (the count-slot sizing pitfall he flagged) + `Iter_Add`/`Iter_Remove`/… aliases. |
| #13/#14 `STOP` naming | only `HOOK_STOP`/`ITER_STOP` | added `HOOK_BREAK`/`HOOK_BREAK_1`/`ITER_BREAK` mirroring pawn's `break` (STOP names kept). |
| #5/#12 hook return combining (last-value silently dropped an earlier claim, the `/help` double-output) | last chain value wins | seed + operator from `hook default`: default 0 **OR**s the returns (1 if any hook claims), default 1 **AND**s them (YSI parity); `HOOK_STOP`/`_1` still claim-and-stop. Tests `hook_combine_or`/`hook_combine_and`, suite 269/2. |

## Verified on a live host

The fixes and the hook semantics were re-run against a real open.mp 1.5.8 server,
not just the in-repo harness, because `pawnruns` lacks `format`/`strcat`. The
set operations are pure-Pawn stocks (a C-native plugin was tried and dropped: a
native call crosses the VM boundary and that cost outweighs the work on the
small sets this is used on, so the stocks are as fast or faster), so no plugin
is involved. All
scenarios behaved correctly: the removal matrix (remove the current, every, or a
future element), `Reverse`, multi-dim rows, `setalloc`/`setfree`, nested
`foreach`, `break`, and `sethas` mid-loop. On the hook side (the area Y-Less
deep-dived): priority runs higher-first, a call-site `continue()` reaches the
original at the tail, zero `continue()` replaces it and calling it twice runs
the original twice, a callback chain runs in priority order with the user's own
`public` as the last link, and `HOOK_STOP_1` claims a value and halts the rest
of the chain. (This run predated the return-combining fix below; the combine is
pure in-AMX bytecode with no host-only natives, so the `hook_combine_*` runtime
tests are authoritative for it.) No new divergences surfaced.

## Deliberate differences (with upgrade paths)

**Hook default return (his comment).** pawn-x defaults a hook chain to `0` (pawn's
natural default); YSI defaults to `1`. As he noted, `0` is "more logically
correct" but needs a `hook default OnFoo = 1;` for the callbacks that must
confirm (few of them). **Upgrade:** add `hook default <cb> = 1;` for callbacks
where YSI relied on the implicit `1`. Note the combining itself now matches YSI
(default 0 ORs, default 1 ANDs), so only the implicit default differs, see the
Fixed table above.

**Hook priority order (#12).** pawn-x runs **higher priority first** (`hook:100`
before `hook:0`); YSI's `@N` suffix runs the opposite way. **Upgrade:** invert
the numbers when porting `hook Foo@N` → `hook:N Foo`.

**Reserved keywords (#5/#8).** pawn-x reserves its construct keywords
(`foreach`, `hook`, `task`, `ptask`, `async`, `await`, `yield`, `inline`,
`iterfunc`) rather than using `__`-prefixed names, the clean syntax is the
point of doing this in the compiler. The newer string-hash intrinsics
(`hash`/`ihash`/`fnv1`/`fnv1a`) are **call-position soft** (identifiers except
directly before `(`), which is the friendlier model; the older keywords are
downgraded only in declaration positions. **Upgrade:** rename a variable/function
that collides with a reserved keyword (as when adopting any YSI keyword). This is
the accepted trade of a compiler feature over an optional, `YSI_NO_KEYWORD_*`-style
library; his `__hook`+`#define hook __hook` alternative remains a reasonable
future option if collisions prove common.

**Error numbering (#11).** pawn-x's new diagnostics live at `253+` (treated as
non-suppressible errors) because stock Pawn leaves no free contiguous *error*
numbers, only `095` to `099` were open, and those are used for the `yield`
diagnostics. His point that `>= 253` blocks future *suppressible* warnings is
noted as a known constraint; a dedicated high error range is a possible future
refactor, deferred because renumbering would churn every error-number test and
the published error tables.

## Considered, deferred (design, not fixes)

- **Decorators** (`@hook(2)`, `@timer(1000)`) as an alternative to the `:`/`[]`
  syntaxes, a larger, orthogonal syntax system; noted, not adopted now.
- **Full YSI coexistence** (his point that YSI keywords are disableable), pawn-x
  is currently a standalone replacement by design; running both at once is out of
  scope for now.
- **Postfix vs infix state syntax** (`hook Foo() <a:b>` vs `hook <a:b> Foo()`):
  the infix form is canonical; the postfix form is accepted where unambiguous.

## Thanks

The review was generous and specific. The removal-safe `foreach`, the
original-`public` chain, and the crash guard are direct results of it.
