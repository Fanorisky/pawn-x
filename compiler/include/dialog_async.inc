/* dialog_async.inc -- NATIVE port of the samp-pp-dialogs idea (await a dialog),
 * built on pawn-x native async instead of PawnPlus tasks. No plugin.
 *
 * PawnPlus version:
 *     new Task:t = ShowPlayerAsyncDialog(playerid, style, caption, info, b1, b2);
 *     new responses[E_ASYNC_DIALOG];
 *     await_arr(responses) t;
 *
 * Native version (this file):
 *     async Menu(playerid)
 *     {
 *         if (await Dialog_Show(playerid, DIALOG_STYLE_LIST, "Menu", "A\nB\nC", "OK", "Back"))
 *             printf("picked item %d (%s)", Dialog_Listitem(playerid), Dialog_Input(playerid));
 *     }
 *     Async_Start(Menu, playerid);
 *
 * Mechanism: "await Dialog_Show(...)" arms ShowPlayerDialog and parks the coroutine
 * on its OWN token (Async_Self, like async_omp.inc's timer-await). When the dialog
 * comes back, OnDialogResponse calls Dialog_Resolve(...), which stashes the response
 * fields per-player and Async_Resume()s the coroutine. The await expression yields
 * the response flag (0/1); the multi-field result (listitem + inputtext) is read
 * back with the Dialog_* accessors -- the native idiom for a multi-field awaitable
 * (the inbox carries one cell; arrays/strings ride in the per-player buffer).
 *
 * A production build would intercept OnDialogResponse with pawn-x's native "hook"
 * keyword instead of asking the caller to forward it; this PoC keeps the forward
 * explicit so the whole flow is visible in one file.
 */

#include "async_omp.inc"

#if !defined MAX_PLAYERS
    #define MAX_PLAYERS (1000)
#endif
#if !defined DIALOG_ASYNC_ID
    #define DIALOG_ASYNC_ID (30000)     /* the dialog id this adapter owns */
#endif
#define DIALOG_ASYNC_INPUT_LEN (128)

enum E_DIALOG_ASYNC
{
    bool:e_dlg_response,
    e_dlg_listitem,
    e_dlg_input[DIALOG_ASYNC_INPUT_LEN]
}
static g_dlgResult[MAX_PLAYERS][E_DIALOG_ASYNC];
static g_dlgToken[MAX_PLAYERS];         /* 0 = no dialog pending for this player */

/* arm the dialog and return this coroutine's token so "await" parks on it */
stock __dlg_show(playerid, style, const caption[], const info[], const b1[], const b2[], token)
{
    g_dlgToken[playerid] = token;
    ShowPlayerDialog(playerid, DIALOG_ASYNC_ID, DIALOG_STYLE:style, caption, info, b1, b2);
    return token;
}
#define Dialog_Show(%0,%1,%2,%3,%4,%5) __dlg_show((%0),(%1),(%2),(%3),(%4),(%5),Async_Self())

/* call this from OnDialogResponse: resolve the awaiting coroutine for `playerid`.
 * returns 1 if it consumed the response (a coroutine was waiting), else 0. */
stock Dialog_Resolve(playerid, response, listitem, const inputtext[])
{
    new token = g_dlgToken[playerid];
    if (token == 0)
        return 0;                       /* nobody awaiting -> let other handlers run */
    g_dlgToken[playerid] = 0;
    g_dlgResult[playerid][e_dlg_response] = bool:(response != 0);
    g_dlgResult[playerid][e_dlg_listitem] = listitem;
    new i = 0;
    while (i < DIALOG_ASYNC_INPUT_LEN-1 && inputtext[i] != '\0')
    {
        g_dlgResult[playerid][e_dlg_input][i] = inputtext[i];
        i++;
    }
    g_dlgResult[playerid][e_dlg_input][i] = '\0';
    Async_Resume(token, response);      /* await expression yields the response flag */
    return 1;
}

/* accessors for the fields the single-cell await could not carry */
stock bool:Dialog_Response(playerid) return g_dlgResult[playerid][e_dlg_response];
stock Dialog_Listitem(playerid)      return g_dlgResult[playerid][e_dlg_listitem];
/* copy the input text into `dest`; returns dest length */
stock Dialog_Input(playerid, dest[], size = sizeof dest)
{
    new i = 0;
    while (i < size-1 && g_dlgResult[playerid][e_dlg_input][i] != '\0')
    {
        dest[i] = g_dlgResult[playerid][e_dlg_input][i];
        i++;
    }
    dest[i] = '\0';
    return i;
}
