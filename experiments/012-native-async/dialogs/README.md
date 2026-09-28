# Native port of samp-pp-dialogs (await a dialog, no plugin)

**Question:** can [Hreesang/samp-pp-dialogs](https://github.com/Hreesang/samp-pp-dialogs)
(a PawnPlus library that makes `ShowPlayerDialog` awaitable) be ported to pawn-x
native async, with no PawnPlus and no plugin?

**Answer:** yes, and it's a natural fit. A dialog is the textbook callback-await:
show it, park the coroutine, resume from `OnDialogResponse`. That's the exact shape
`async_omp.inc` already uses for timers, so the port is a thin adapter.

## Mapping

| PawnPlus (samp-pp-dialogs) | pawn-x native (`dialog_async.inc`) |
|---|---|
| `Task:ShowPlayerAsyncDialog(...)` | `await Dialog_Show(playerid, style, caption, info, b1, b2)` |
| `await_arr(responses) t;` | `await` yields the response flag; `Dialog_Listitem()` / `Dialog_Input()` read the rest |
| PawnPlus Task + frame snapshot (plugin) | compiler coroutine + `Async_Resume` seam (no plugin) |
| resolves inside the plugin's dialog hook | `OnDialogResponse` → `Dialog_Resolve(...)` (or pawn-x native `hook`) |

The one shape difference: PawnPlus `await_arr` hands back a whole array; a native
`await` yields a single cell. So the adapter yields the response flag through the
await and parks the multi-field result (listitem + inputtext) in a per-player
buffer read via `Dialog_Listitem()` / `Dialog_Input()`. Same ergonomics, one extra
accessor call.

## Files
- `../../../compiler/include/dialog_async.inc`, the adapter (built on `async_omp.inc`).
- `dialog_demo.pwn`, a single-dialog flow (player 0) and a chained two-step login
  flow (player 1). Since no game client is connected, the player's answers are
  simulated with real open.mp timers that call `Dialog_Resolve`.

## Host result (open.mp 1.5.8, `build/pawncc`, no plugin)

```
>>> NATIVE async dialogs (samp-pp-dialogs port, pawn-x coroutine, no plugin)
[dialog]  coroutine armed a dialog, awaiting the player's response...
[dialog]  player 0 picked item 2, input='hello' -- coroutine resumed
[login]   player 1 -> name='Ada' pass='s3cret' (both survived across two awaits)
```

The login line is the payoff: two sequential dialogs read as straight-line code,
and the first dialog's result (`name`) is still in scope after the second `await`:
the coroutine's lifted locals survive both suspends. No nested callbacks, no
plugin.

## Caveats / production notes
- The PoC forwards `OnDialogResponse` explicitly; a real library would intercept it
  with pawn-x's native `hook` keyword (also plugin-free) so the caller's own
  `OnDialogResponse` still runs.
- One dialog id (`DIALOG_ASYNC_ID`) and one pending slot per player, like the
  original (a new dialog while one is awaiting discards the old wait).
- PawnPlus string variants (`ConstString:`) aren't ported; native uses plain
  packed strings.
