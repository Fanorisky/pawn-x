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

## API mapping

### Varargs (`y_va` → `___`)

| YSI | pawn-x |
|---|---|
| `f(const fmt[], va_args<>)` | `f(const fmt[], ...)` |
| `va_printf(fmt, va_start<1>)` | `printf(fmt, ___)` |
| `va_format(dst, size, fmt, va_start<3>)` | `format(dst, size, fmt, ___)` |
| `strcpy(dst, va_return(fmt, va_start<3>), size)` | `format` into a local and `return` it |
| `va_SendClientMessage(id, col, fmt, ...)` | `SendClientMessage(id, col, Fmt(fmt, ___))` |

`va_start<N>` = `___(N)` — forward from the N-th (0-based) variable argument;
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
| — (no scalar equivalent) | `setget(name, index)` |
| `foreach (new v : Name)` | `foreach (new v : name)` |
| `foreach (new v : Reverse(Name))` | `foreach (new v : Reverse(name))` |

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
| *(no runtime equivalent)* | `dynhook_add/remove/replace` + `dynhook_intercept` |

## Gotchas

- **Don't mix.** Including any YSI `y_va`/`y_iterate`/`y_hooks` alongside pawn-x
  is a hard compile error (guarded in the includes).
- **`dynhook` needs the companion plugin** loaded on the server; the compiler
  `hook` keyword and everything else need no plugin.
- **Other YSI libraries** (`y_commands`, `y_ini`, `y_inline`, `y_timers`,
  `y_groups`, …) are out of scope — keep using them or an open.mp alternative;
  they don't clash with pawn-x.
