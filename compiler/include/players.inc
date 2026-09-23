#if defined _INC_players
	#endinput
#endif
#define _INC_players

/**
 * <library name="players">
 *  Ready-made connected-players iterator — pawn-x's batteries-included answer to
 *  YSI's `Iterator:Player`. `Player` is a compact set kept in sync by hooking the
 *  connect/disconnect callbacks, so you can just:
 *
 *    foreach (new i : Player) { ... }   // every connected player, ascending id
 *
 *  Needs the `iterset` companion plugin (for the set* natives) and open.mp's
 *  callback dispatch. Built on the native `foreach`/`set*`/`hook` — no macros,
 *  no bytecode scan. Vehicle/Actor iterators are not shipped here because their
 *  lifecycle is native-driven (CreateVehicle/DestroyVehicle), which pawn-x does
 *  not hook; use tracked wrappers for those.
 * </library>
 */

#include <foreach>
#include <hook>

#if !defined MAX_PLAYERS
	#define MAX_PLAYERS (1000)
#endif

// slot 0 is the count; up to MAX_PLAYERS distinct ids
new Player[MAX_PLAYERS + 1];

hook OnPlayerConnect(playerid)
{
	setadd(Player, playerid);
	return HOOK_CONTINUE;
}

hook OnPlayerDisconnect(playerid, reason)
{
	#pragma unused reason
	setremove(Player, playerid);
	return HOOK_CONTINUE;
}

// Seed players already connected when this script loads (relevant for
// filterscripts / hot reloads). Guarded so it only compiles where the host
// provides IsPlayerConnected; define PLAYERS_NO_SEED to skip it entirely.
#if !defined PLAYERS_NO_SEED
hook OnGameModeInit()
{
	for (new i = 0; i < MAX_PLAYERS; i++)
		if (IsPlayerConnected(i))
			setadd(Player, i);
	return HOOK_CONTINUE;
}
#endif
