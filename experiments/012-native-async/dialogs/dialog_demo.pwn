#include <open.mp>
#include <dialog_async>

// NATIVE port of samp-pp-dialogs: await a dialog with a compiler coroutine, no
// PawnPlus, no plugin. Same ergonomics as the PawnPlus original -- show a dialog
// and let the flow "pause" until the player answers -- but the suspend is the
// pawn-x compiler's coroutine, resumed from OnDialogResponse via the Async_Resume
// seam (dialog_async.inc), exactly like the timer-await in async_omp.inc.

async Menu(playerid)
{
    print("[dialog]  coroutine armed a dialog, awaiting the player's response...");
    // "await Dialog_Show(...)" parks HERE until OnDialogResponse resolves it; the
    // await yields the response flag, the rest of the fields ride in the buffer.
    new ok = await Dialog_Show(playerid, DIALOG_STYLE_LIST, "Menu", "Apple\nBanana\nCherry", "OK", "Back");
    if (ok)
    {
        new input[64];
        Dialog_Input(playerid, input);
        printf("[dialog]  player %d picked item %d, input='%s' -- coroutine resumed",
               playerid, Dialog_Listitem(playerid), input);
    }
    else
        printf("[dialog]  player %d cancelled", playerid);
    return ok;
}

// The real payoff: a MULTI-STEP flow reads as straight-line code, no nested
// callbacks. Two dialogs in sequence, each awaited, the first result still in scope.
async Login(playerid)
{
    await Dialog_Show(playerid, DIALOG_STYLE_INPUT, "Login (1/2)", "Enter your name:", "Next", "");
    new name[64];
    Dialog_Input(playerid, name);
    await Dialog_Show(playerid, DIALOG_STYLE_PASSWORD, "Login (2/2)", "Enter your password:", "Login", "Back");
    new pass[64];
    Dialog_Input(playerid, pass);
    printf("[login]   player %d -> name='%s' pass='%s' (both survived across two awaits)", playerid, name, pass);
    return 1;
}

// LIST dialog: the response flag comes from the await, the chosen row from
// Dialog_Listitem() -- same two-line shape as reading inputtext.
async MenuList(playerid)
{
    if (await Dialog_Show(playerid, DIALOG_STYLE_LIST, "Pilih Buah", "Apple\nBanana\nCherry", "OK", "Batal"))
        printf("[list]    player %d listitem=%d (Dialog_Listitem accessor)", playerid, Dialog_Listitem(playerid));
    else
        printf("[list]    player %d cancelled", playerid);
    return 1;
}

// Real dialog responses land here. A production port would intercept this with
// pawn-x's native "hook" keyword; the PoC forwards it explicitly.
public OnDialogResponse(playerid, dialogid, response, listitem, inputtext[])
{
    if (dialogid == DIALOG_ASYNC_ID)
        return Dialog_Resolve(playerid, response, listitem, inputtext);
    return 0;
}

// Headless validation: no game client is connected, so simulate the player's
// answers via real open.mp timers (each fires Dialog_Resolve for whatever dialog
// is currently pending for that player).
forward SimMenu();
forward SimLogin1();
forward SimLogin2();
forward SimList();
public SimMenu()   { Dialog_Resolve(0, 1, 2, "hello"); }
public SimLogin1() { Dialog_Resolve(1, 1, 0, "Ada"); }
public SimLogin2() { Dialog_Resolve(1, 1, 0, "s3cret"); }
public SimList()   { Dialog_Resolve(2, 1, 1, ""); }        // OK, listitem 1 (Banana)

public OnGameModeInit()
{
    print(">>> NATIVE async dialogs (samp-pp-dialogs port, pawn-x coroutine, no plugin)");
    Async_Start(Menu, 0);                    // single dialog for player 0
    SetTimer("SimMenu", 300, false);         // player 0 answers @300ms

    Async_Start(Login, 1);                   // chained login flow for player 1
    SetTimer("SimLogin1", 400, false);       // answers dialog 1 @400ms
    SetTimer("SimLogin2", 700, false);       // ...then dialog 2 @700ms

    Async_Start(MenuList, 2);                // one-liner list dialog for player 2
    SetTimer("SimList", 500, false);         // answers @500ms
    return 1;
}
main() {}
