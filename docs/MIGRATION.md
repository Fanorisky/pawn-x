# Migrating from YSI to pawn-x

pawn-x replaces YSI's language-tamer core (`y_va`, `y_iterate`/`y_foreach`,
`y_hooks`). It is a **standalone** replacement: reserve `foreach`/`hook` as
keywords, so you remove the YSI includes rather than keep them alongside.

Swap the includes:

```pawn
// remove
#include <YSI_Coding\y_va>
#include <YSI_Data\y_iterate>
#include <YSI_Coding\y_hooks>
// add
#include <pawn-x>
```

## What behaves differently (by design)

pawn-x is a native, cleaner take on YSI's script layer, not a byte-for-byte
drop-in. A few behaviors differ on purpose. Each has an upgrade path so porting
stays predictable.

**Hook default return is 0, not 1.** A hook chain with no `hook default` seeds
at 0 and ORs the hook returns, so it returns 1 if any hook claims the callback,
matching YSI's default-0 behaviour. YSI's overall implicit default is 1; the
couple of callbacks that need that get `hook default OnFoo = 1;`, which seeds at
1 and ANDs the chain (returns 1 only if every hook agrees), again like YSI. So
the combining matches YSI, only the implicit default differs. `HOOK_STOP` /
`HOOK_STOP_1` end the chain early with a forced 0 / 1.

**Priority runs higher-first, the opposite of YSI's `@N`.** `hook:100` runs
before `hook:0`; YSI's `@N` suffix runs the other way. A higher number reads as
more important, so it goes first. Upgrade: invert the numbers when porting
`hook Foo@N` to `hook:N Foo`.

**Construct keywords are reserved, so pawn-x and YSI cannot run together.**
`foreach`, `hook`, `task`, `ptask`, `inline`, and `iterfunc` are real compiler
keywords, not optional `__`-prefixed macros. The clean syntax is the whole point of
moving this into the compiler. The async keywords `async`, `await`, and `yield` are
opt-in: they are off by default (ordinary identifiers) and turn on only when their
include emits a pragma, `#include <async>` for `async`/`await`
(`#pragma pawnx_async`) and `#include <foreach>` for `yield` (`#pragma pawnx_yield`).
The newer hash intrinsics (`hash`/`ihash`/`fnv1`/`fnv1a`) are soft in call position
(usable as identifiers except directly before `(`). Upgrade: rename a variable
or function that collides, and remove the YSI includes (the pawn-x includes
detect YSI and stop with a clear error).

**New diagnostics are numbered 253+.** Stock Pawn leaves no free contiguous
error slots, so pawn-x's new errors live at 253 and up. The known trade-off,
raised by YSI's author, is that this range blocks future suppressible warnings;
a dedicated high error range is a possible later refactor.

Everything YSI's author flagged as an actual bug (removal-safe `foreach`,
calling your own `public` when it is also hooked, the empty-state guard, `set*`
const-correctness) was fixed, not kept. See [YSI-NOTES](YSI-NOTES.md) for the
full point-by-point.

## API mapping

### Varargs (`y_va` → `___`)

| YSI | pawn-x |
|---|---|
| `f(const fmt[], va_args<>)` | `f(const fmt[], ...)` |
| `va_printf(fmt, va_start<1>)` | `printf(fmt, ___)` |
| `va_format(dst, size, fmt, va_start<3>)` | `format(dst, size, fmt, ___)` |
| `strcpy(dst, va_return(fmt, va_start<3>), size)` | `format` into a local and `return` it |
| `va_SendClientMessage(id, col, fmt, ...)` | `SendClientMessage(id, col, Fmt(fmt, ___))` |

`va_start<N>` = `___(N)`, forward from the N-th (0-based) variable argument;
bare `___` forwards all of them.

### Iterators (`y_iterate`/`y_foreach` → `foreach` + `set*`)

| YSI | pawn-x |
|---|---|
| `new Iterator:Name<cap>` | `new name[cap]` |
| `new Iterator:Name[OUTER]<cap>` | `new name[OUTER][cap]` |
| `Iter_Add(Name, v)` | `setadd(name, v)` |
| `Iter_Remove(Name, v)` | `setremove(name, v)` |
| `Iter_Contains(Name, v)` | `sethas(name, v)` |
| `Iter_Count(Name)` | `setlen(name)` |
| `Iter_Clear(Name)` | `setinit(name)` |
| `Iter_Free(Name)` | `setfree(name)` |
| `Iter_Alloc(Name)` | `setalloc(name)` |
| `Iter_Random(Name)` | `setrandom(name)` |
| (no scalar equivalent) | `setget(name, index)` |
| `foreach (new v : Name)` | `foreach (new v : name)` |
| `foreach (new v : Reverse(Name))` | `foreach (new v : Reverse(name))` |
| `Iterator:Player` (auto-maintained) | `#include <players>` → `Player` |
| `Iterator:Vehicle` / `Actor` | `#include <vehicles>` / `<actors>` → `Vehicle_Create`/`Actor_Create` wrappers |
| custom `iterfunc` needing state (Fib) | `iterfunc Name(&acc, cur, ...)` (leading ref = persistent state) |
| `#define Iterator@N iteryield` + `iterfunc N() { yield return x; }` | `iterfunc N() { yield return x; }` (no `iteryield` define, pawn-x detects `yield`) |

Note the model difference: YSI is an **index-set** (values must be `< cap`);
pawn-x is a **value-set** (sorted distinct values, any magnitude the array
holds). For ids in `[0, cap)` they behave identically. Removing the currently
iterated element is safe in `foreach` (it hangs under YSI on this stack).

### Hooks (`y_hooks` → `hook` / `dynhook`)

| YSI | pawn-x |
|---|---|
| `hook Name(args) { }` + `#include <y_unique>` between bodies | `hook Name(args) { }` (no boilerplate) |
| `return Y_HOOKS_CONTINUE_RETURN_1` | `return HOOK_CONTINUE` |
| `return Y_HOOKS_CONTINUE_RETURN_0` | `return HOOK_CONTINUE_0` |
| `return Y_HOOKS_BREAK_RETURN_0` | `return HOOK_STOP` |
| `return Y_HOOKS_BREAK_RETURN_1` | `return HOOK_STOP_1` |
| `PRE_HOOK` / `CHAIN_ORDER` priority | `hook:N Name(args) { }` (higher N first) |
| `HOOK_RET:OnPlayerCommandText(){return 0;}` | `hook default OnPlayerCommandText = 0;` |
| `hook function Name(args) { }` (call-site) | `hook function Name(args) { }` (native call redirection, no code-segment rewrite) |
| `hook native Name(args) { }`, YSI alias of `hook function` (target kind not enforced) | `hook native Name(args) { }` |
| `hook stock Name(args) { }`, YSI alias of `hook function` (target kind not enforced) | `hook stock Name(args) { }` |
| `continue(args)` (call the original / next) | `continue(args)` (0×=replace, 1×=pass-through, N×=call original N times; args forwardable) |
| `hook native Name(fmt[], ...) { }` (variadic target) | `hook function Name(fmt[], ...) { }` (variadic call-hook, same `fixed…, ...` shape as the target) |
| `continue(a, va_start<1>)` (forward the tail) | `continue(a, ___)`, pass fixed args then `___` to splice the whole tail; bare `continue()` forwards everything |
| `Hooks_NumArgs()` (chain arg count, hides the wrapper's extra param) | `numargs()` (already corrected: the hidden per-hook chain index is invisible, so `numargs()`/`getarg(n)`/`setarg(n)` use the USER index) |
| *(no runtime equivalent)* | `dynhook_add/remove/replace` + `dynhook_intercept` |

In YSI 5.10 `hook native` and `hook stock` are **documented synonyms** of
`hook function` (see y_hooks' "Synonyms": they share one FUNC_PARSER-based
`HOOK_*__` expansion, so the modifier does not select or check the target's
kind). pawn-x instead makes the modifier meaningful: `hook native` requires a
real native (kind-checked, reached by a direct SYSREQ), while
`hook function`/`hook stock` require a pawn function/stock. A wrong modifier is
a compile error (see the call-site-hook gotcha below).

## Async / await (`y_async` → native `async`/`await`)

**YSI never shipped `y_async`.** It is an *abandoned sketch*: `y_async_impl.inc`
has an empty `_Async_A()` stub and bare design-note statements at file scope that
do not compile; the enabling macros are half-built and Y_Less stopped maintaining
YSI. So there is no working YSI runtime to migrate *from*. pawn-x is the first to
actually implement `async`/`await`. The table below maps YSI's *intended* syntax
(from its docs/sketch) to what pawn-x provides.

Add the include (it is not part of the umbrella `<pawn-x>`). This include is also
the opt-in: `async`/`await` are off by default and are ordinary identifiers until
`<async>` emits `#pragma pawnx_async`, so a script that never includes it can name a
variable `async` or `await` (and can use PawnPlus's own `async`/`await` syntax
instead). The generator `yield` keyword is opt-in the same way, enabled by
`#include <foreach>` (`#pragma pawnx_yield`).

```pawn
#include <async>
```

| YSI `y_async` (sketched) | pawn-x (native, working) |
|---|---|
| `async Func() { }` | `async Func() { }` (compiler keyword) |
| `new r = await Op();` | `new r = await Op();`, suspend, resume with the result |
| `await AsyncFunc(args)` (compose) | `await AsyncFunc(args)`, inner `return` resumes the awaiter |
| *(scheduler unspecified)* | `new t = Async_Start(Fn, args);`, start, get a token |
| *(completion unspecified)* | `Async_Resume(token, value)`, deliver result, resume |
| *(no accounting)* | `Async_ActiveCount()`, live-coroutine count (lifetime check) |
| leaf awaitable | `await Async_Pending()` (self-register) or `await 0` (resumed by the start token) |
| `await X() -> (a, b, …)` (multi-result bind) | **not in MVP**: single scalar result only |

Completion is driven by `Async_Resume(token, value)`: a synthetic pump in tests
today; a real timer/dialog/DB/HTTP host adapter is a thin wrapper that calls the
same entry (out of MVP scope). `async`/`await` is single-`.amx` and linear-body in
this MVP, but scalar **and** array/string/multi-dim LOCALS now survive an `await`
(they are lifted into the coroutine's own state block). What still doesn't cross an
`await`: array / `&`reference PARAMS → error 268 (they point into the caller's
frame); `await` inside a `foreach` → error 099; `foreach` over an `async` function
→ error 267. See the header comment in `include/async.inc`.

### vs PawnPlus `amx_async` / coroutines

PawnPlus also offers `await`-style coroutines, but as a **runtime plugin**: it
suspends by taking a full snapshot of the AMX machine (`amx::reset` `memcpy`s the
whole stack + heap for that call) and restores it on resume. pawn-x needs **no
plugin**. The compiler emits the state machine and copies only *the coroutine's
own lifted locals* into a fixed data-segment block, so there is nothing to
save/restore of the surrounding stack/heap. That is leaner and plugin-free; the
trade-off is the documented compile-time limits above. Operator temporaries
mid-expression are preserved (spilled into the state block and restored on resume),
so `base + await F()`, multiple awaits in one statement (`await A() + await B()`), a
leaf `await` as a **fixed-arity** call argument, and **fixed-size array/string
parameters** (copied into the coroutine block) all work. What still needs the
plugin's whole-frame snapshot: **unsized**/multi-dim/`&`reference params, a leaf
`await` inside a **variadic** call's arguments, a composed-then-leaf await pair in one
expression, and suspending at arbitrary
call-stack depth inside a non-async helper.

**Combinators and faults are now native too.** `Async_All(n)` / `Async_Any(n)` +
`await Async_Wait(g)` fan several outstanding operations into one awaiting
coroutine (PawnPlus `task_all` / `task_any` parity): a coroutine creates a gate,
kicks off its operations, and parks until all (sum of results) or the first (its
result) complete. Faults map `task_set_error`: `Async_ResumeError(token, err)` /
`Async_GateFail(gate, err)` report failure, which the coroutine observes with
`Async_Failed()`/`Async_Error()` right after the await (leaf-await model). Like
everything else this is pure library code over the `Async_GateFeed(g, value)` /
`Async_Resume` seam, no plugin, no compiler change, no new opcode.

**A real host adapter now ships and is validated on a live open.mp server.**
`async_omp.inc` bridges the resume seam to open.mp `SetTimerEx`, giving real
awaitables: `await Async_Ms(1000)` suspends on an actual server timer, and a
`public` callback resuming via `Async_Resume`/`Async_GateFeed` is the callback-await
pattern. Validated on open.mp 1.5.8 (Timers.so): five coroutines park on real
timers, resume in correct chronological order off the tick loop, with scalar +
array locals surviving the suspend, combinators and the fault channel working, and
the arena returning to baseline, no plugin, no leak (see
`experiments/012-native-async/HOST-VALIDATION.md`). Composed `await asyncFn()`
works **mid-expression** (`base + await Work()`) and **inside a loop**
(`for (…) total += await Step(i);`); a leaf `await` works as a **fixed-arity** call
argument (`foo(await F(), p, q)`, any position); **multiple awaits** may appear in one
statement (`await A() + await B()`); **fixed-size array/string parameters** are copied
into the coroutine block; and **multi-result** delivery
(`Async_ResumeArr`/`Async_InboxArr`) gives `await_arr` parity. pawn-x still does
**not** match PawnPlus's remaining breadth: IMPLICIT fault auto-raise (a leaf fault an
inner ignored does not raise by itself, the inner calls
`Async_Fail(err)` to propagate, which IS supported and propagates up the compose
chain), suspending at arbitrary call-stack depth (nested non-async frames), unsized/
multi-dim/`&`reference parameters across an await, a leaf `await` inside a **variadic**
call's arguments, and a composed-then-leaf await pair in one expression (error
268/099), plus JIT compatibility, all remain roadmap/non-goal.



- **Don't mix.** Including any YSI `y_va`/`y_iterate`/`y_hooks` alongside pawn-x
  is a hard compile error (guarded in the includes).
- **`dynhook` needs the companion plugin** loaded on the server; the compiler
  `hook` keyword and everything else need no plugin.
- **Call-site hooks (`hook native/function/stock`) are single-`.amx`.** Both
  fixed-arity and variadic (`...`) targets are supported: the body's arg list
  must match the target's signature exactly, including its variadic shape
  (`fixed…, ...` vs `fixed…`), and the modifier must match the target's kind.
  All mismatches are compile errors. Inside a variadic body forward the tail
  with `continue(fixed…, ___)` or everything with bare `continue()`;
  `numargs()`/`getarg(n)`/`setarg(n)` use the user index (the hidden chain index
  is invisible). Cross-`.amx` call-site hooking is out of scope (like the
  callback `hook`).
- **YSI's app libraries** (`y_commands`, `y_ini`, `y_inline`, `y_timers`,
  `y_groups`, ...) are being rebuilt on the native core, not kept alongside.
  `y_inline` and `y_timers` already have native equivalents (`inline`,
  `task`/`ptask`); the rest are ported as `x_`-prefixed wrapper libraries.
  `x_commands` and `x_ini` are the first two. See the App layer section below.

## App layer (`y_*` → `x_*`)

Because pawn-x owns the core keywords, YSI cannot run alongside it, so the
libraries that used to sit on top of YSI are rewritten on the native core.
They keep an `x_` prefix mirroring YSI's `y_`, so porting is mostly a rename.
Each is an explicit include; they are not pulled in by `<pawn-x>`.

### Commands (`y_commands` → `x_commands`)

`#include <x_commands>`. This is a drop-in: the handler syntax is the y_commands
one, `YCMD:name(playerid, params[], help)`, and the ZCMD-style `CMD:` and
`COMMAND:` (two params, no help) also work. In expression position `YCMD:name`
resolves to the command id, so it feeds straight into x_groups.

```pawn
YCMD:heal(playerid, params[], help)
{
    if (help) return COMMAND_OK;
    SetPlayerHealth(playerid, 100.0);
    return COMMAND_OK;
}
```

| y_commands | x_commands |
| --- | --- |
| `YCMD:name(playerid, params[], help)` / `CMD:` / `COMMAND:` | same names |
| `YCMD:name` as an id (e.g. for groups) | same, resolves to `Command_GetID("name")` |
| `e_COMMAND_ERRORS` (`COMMAND_OK`, `COMMAND_UNDEFINED`, ...) | same enum, same values |
| `OnPlayerCommandReceived` / `OnPlayerCommandPerformed` | same callbacks, same tags |
| `Command_GetID` / `Command_SetDisabled` / `Command_SetHidden` / `Command_SetPlayerDisabled` | same names, by command id |
| `Command_AddAltNamed` / `Command_SetUnknownReturn` | same names |

The dispatcher hooks `OnPlayerCommandText`, parses the command, resolves
aliases, checks the per-command / per-player state, and, when `<x_groups>` is
included before it, gates by `Group_PlayerAllowed(playerid, cmdid)` returning
`COMMAND_DENIED`. Not carried over: the multi-script master and the `#emit`
fast-call path, which are y_commands internals a single module does not need.

### INI (`y_ini` → `x_ini`)

`#include <x_ini>`. This is a drop-in: the declarative loader syntax is the
same as y_ini. You write an `INI:file[tag](name[], value[])` callback and call
`INI_Load("file")`, and each `key = value` line under `[tag]` reaches your
callback, where the readers match a key and return.

```pawn
INI:users[data](name[], value[])
{
    INI_Int("score", gScore);
    INI_Float("health", gHealth);
    INI_String("name", gName);
    return 1;
}
// load: INI_Load("users");
```

The reader macros are the y_ini ones: `INI_Int`, `INI_Float`, `INI_Hex`,
`INI_Bin`, `INI_Bool`, `INI_String`. Writing uses the same buffered handle
API, `INI_Open` returning an `INI:` handle down to `INI_Close`:

| y_ini | x_ini |
| --- | --- |
| `INI:file[tag](name[], value[])` + `INI_Load` / `INI_ParseFile` | same names, same shape |
| `INI_Int` / `INI_Float` / `INI_Hex` / `INI_Bin` / `INI_Bool` / `INI_String` | same names |
| `INI_Open` / `INI_SetTag` / `INI_Close` / `INI_NO_FILE` | same names |
| `INI_WriteString` / `WriteInt` / `WriteFloat` / `WriteBool` / `WriteHex` / `WriteBin` | same names |
| `INI_RemoveEntry` / `INI_DeleteTag` | same names |

What differs is underneath, not in the source: the writer maps onto open.mp's
config natives (`writecfg` / `writecfgvalue` / `deletecfg`) instead of YSI's
temp-file rewrite, so a write needs no plugin. The loader reads the file with
the file natives and dispatches with `CallLocalFunction`. Not yet matched:
tag inheritance (`[child] : parent`) and exact comment-preserving rewrites.

### Groups (`y_groups` → `x_groups`)

`#include <x_groups>`. This is a drop-in: same `Group:` tag, same
`ALLOW`/`DENY`/`UNDEF` constants (tag `E_GROUP_SET`), same `GROUP_GLOBAL`. A
group is a set of players; each group holds a tri-state permission over an
"entity", any integer id the caller assigns. A command id is an entity, so
`YCMD:name` passes straight into the command family. Everyone is implicitly in
`GROUP_GLOBAL`, whose per-entity default is `ALLOW`. The model is plain Pawn,
no plugin and no host native.

```pawn
new Group:gAdmins = Group_Create("admins");
Group_SetPlayer(gAdmins, playerid, true);
Group_SetCommand(gAdmins, YCMD:ban, ALLOW);
// gate a command: if (!Group_PlayerAllowed(playerid, YCMD:ban)) return 0;
```

| y_groups | x_groups |
| --- | --- |
| `Group:` tag, `GROUP_GLOBAL`, `ALLOW`/`DENY`/`UNDEF` (`E_GROUP_SET`) | same names and tag |
| `Group_Create` / `Group_Destroy` / `Group_IsValid` / `Group_GetID` | same names, `Group_Create` returns `Group:` |
| `Group_SetPlayer` / `Group_GetPlayer` / `Group_GetCount` / `Group_SetName` | same names |
| `Group_SetCommand` / `Group_GetCommand` / `Group_CommandAllowed` / `Group_CommandDenied` | same names |
| `Group_SetGlobalCommand` / `Group_GlobalCommandAllowed` / `Group_SetGlobalCommandDefault` | same names |

Resolution matches y_groups: a `DENY` in any of the player's groups (or the
global group) blocks the entity outright, otherwise a single `ALLOW` permits
it, and if nothing is set explicitly the global default decides
(`Group_SetGlobalDefault`). `Group_PlayerAllowed(playerid, entity)` /
`Group_PlayerDenied` run that resolution; `Group_GetName` fills a buffer
(`Group_GetName(g, dest[], size)`) rather than returning a string inline. The
library hooks `OnPlayerDisconnect` to clear a leaving player's membership
(define `X_GROUPS_STANDALONE` to drop that hook where host callbacks do not
exist). Not ported: the `Group_Set<Lib>...` metaprogramming that YSI stamps
out per library, group hierarchy, gangs, colours, and balanced groups.
