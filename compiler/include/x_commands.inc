#if defined _INC_x_commands
	#endinput
#endif
#define _INC_x_commands

#include <hook>
#include <string>
#include <hash>
#include <foreach>

/**
 * <library name="x_commands">
 *  Native-backed command system, the pawn-x app-layer replacement for YSI's
 *  y_commands (single-module; the multi-script master and #emit fast-call that
 *  make up most of y_commands' bulk are not needed here). Declare a handler:
 *
 *    x_cmd:heal(playerid, params[], help)
 *    {
 *        if (help & COMMAND_HELP) { SendClientMessage(playerid, -1, "/heal"); return COMMAND_OK; }
 *        SetPlayerHealth(playerid, 100.0);
 *        return COMMAND_OK;
 *    }
 *
 *  A hook on OnPlayerCommandText parses the command (case-insensitive), splits
 *  the parameter tail, resolves aliases, checks per-command / per-player
 *  permission, runs OnPlayerCommandReceived (veto), dispatches to the handler,
 *  then runs OnPlayerCommandPerformed. The handler's e_COMMAND_ERRORS result and
 *  a settings mask decide whether the "unknown command" message is suppressed.
 *  Requires the host includes (<open.mp> / SA-MP stdlib) first.
 * </library>
 */

#if !defined MAX_X_COMMANDS
	#define MAX_X_COMMANDS (256)
#endif

// dispatch/return protocol (mirrors YSI's e_COMMAND_ERRORS values)
enum e_COMMAND_ERRORS
{
	COMMAND_ZERO_RET    = 0,   // handler returned 0: treated as "not really handled"
	COMMAND_OK          = 1,   // handled
	COMMAND_UNDEFINED   = 2,   // no such command
	COMMAND_DENIED      = 3,   // permission denied
	COMMAND_HIDDEN      = 4,   // exists but hidden from this player
	COMMAND_NO_PLAYER   = 6,   // invalid player
	COMMAND_DISABLED    = 7,   // command disabled
	COMMAND_BAD_PREFIX  = 8,   // wrong prefix
	COMMAND_INVALID_INPUT = 10,
	COMMAND_SILENT      = 11,  // handled, but say nothing
}

// "help" argument bitmask passed to every handler
const COMMAND_HELP   = 1;   // bit 0: a help query (show usage, do not perform)
const COMMAND_BYPASS = 2;   // bit 1: permission checks were bypassed

// x_cmd:name(playerid, params[], help) { ... } -> a public "xcmd_name"
#define x_cmd:%0(%1) forward xcmd_%0(%1); public xcmd_%0(%1)

// Optional pre/post callbacks a script may define (both return e_COMMAND_ERRORS).
forward OnPlayerCommandReceived(playerid, cmdtext[], e_COMMAND_ERRORS:success);
forward OnPlayerCommandPerformed(playerid, cmdtext[], e_COMMAND_ERRORS:success);

// --- state (per-script) -----------------------------------------------------
static stock XCmd_Disabled[MAX_X_COMMANDS + 1];   // set of disabled command hashes
static stock XCmd_Hidden[MAX_X_COMMANDS + 1];     // set of hidden command hashes
static stock XCmd_PlayerOff[1024];                // set of player ids with commands off
static stock XCmd_AliasHash[MAX_X_COMMANDS];      // alias name-hash ...
static stock XCmd_AliasTo[MAX_X_COMMANDS][32];    // ... -> target command name
static stock XCmd_AliasCount = 0;
static stock bool:XCmd_ShowUnknown = true;        // show SA:MP "unknown command" on UNDEFINED?

// Hash a command name (already lowercased by the caller) for the state sets.
static stock XCmd_Hash(const name[])
	return hash(name);

// Look up an alias name-hash; returns its index in the alias table or -1.
static stock XCmd_FindAlias(nameHash)
{
	for (new i = 0; i < XCmd_AliasCount; ++i)
		if (XCmd_AliasHash[i] == nameHash)
			return i;
	return -1;
}

// Lowercase-copy a command name into dst (commands are case-insensitive).
static stock XCmd_Lower(dst[], const src[], len = sizeof dst)
{
	new i = 0;
	for (; src[i] != '\0' && i < len - 1; ++i)
	{
		new c = src[i];
		if ('A' <= c <= 'Z')
			c += 'a' - 'A';
		dst[i] = c;
	}
	dst[i] = '\0';
}

// --- runtime API ------------------------------------------------------------

/// <summary>Does a command by this name exist?</summary>
stock bool:Command_Exists(const name[])
{
	new lo[32], fn[5 + 32] = "xcmd_";
	XCmd_Lower(lo, name);
	strcat(fn, lo);
	return funcidx(fn) >= 0;
}

/// <summary>Disable (or re-enable) a command for everyone.</summary>
stock Command_SetDisabled(const name[], bool:disabled = true)
{
	new lo[32];
	XCmd_Lower(lo, name);
	new h = XCmd_Hash(lo);
	if (disabled)
		setadd(XCmd_Disabled, h);
	else
		setremove(XCmd_Disabled, h);
}
stock bool:Command_GetDisabled(const name[])
{
	new lo[32];
	XCmd_Lower(lo, name);
	return sethas(XCmd_Disabled, XCmd_Hash(lo)) != 0;
}

/// <summary>Hide (or unhide) a command from listings (it still runs).</summary>
stock Command_SetHidden(const name[], bool:hidden = true)
{
	new lo[32];
	XCmd_Lower(lo, name);
	new h = XCmd_Hash(lo);
	if (hidden)
		setadd(XCmd_Hidden, h);
	else
		setremove(XCmd_Hidden, h);
}
stock bool:Command_GetHidden(const name[])
{
	new lo[32];
	XCmd_Lower(lo, name);
	return sethas(XCmd_Hidden, XCmd_Hash(lo)) != 0;
}

/// <summary>Turn all commands off (or back on) for one player.</summary>
stock Command_SetPlayerDisabled(playerid, bool:off = true)
{
	if (off)
		setadd(XCmd_PlayerOff, playerid);
	else
		setremove(XCmd_PlayerOff, playerid);
}
stock bool:Command_GetPlayerDisabled(playerid)
	return sethas(XCmd_PlayerOff, playerid) != 0;

/// <summary>Make `alias` run the handler of `target`.</summary>
stock Command_AddAlt(const alias[], const target[])
{
	if (XCmd_AliasCount >= MAX_X_COMMANDS)
		return 0;
	new lo[32];
	XCmd_Lower(lo, alias);
	XCmd_AliasHash[XCmd_AliasCount] = XCmd_Hash(lo);
	XCmd_Lower(XCmd_AliasTo[XCmd_AliasCount], target);
	return ++XCmd_AliasCount;
}

/// <summary>Whether an unknown command shows SA:MP's "unknown command" (default true).</summary>
stock Command_SetUnknownReturn(bool:show)
	XCmd_ShowUnknown = show;

// --- dispatcher -------------------------------------------------------------
hook OnPlayerCommandText(playerid, cmdtext[])
{
	// parse: optional '/', the command word (lowercased), then the tail
	new i = 0;
	if (cmdtext[i] == '/')
		++i;
	new name[32], n = 0;
	while (cmdtext[i] > ' ' && n < sizeof (name) - 1)
	{
		new c = cmdtext[i];
		if ('A' <= c <= 'Z')
			c += 'a' - 'A';
		name[n++] = c;
		++i;
	}
	name[n] = '\0';
	if (n == 0)
		return HOOK_CONTINUE_0;             // a bare "/" is not a command
	while (cmdtext[i] == ' ')
		++i;
	new params[128], p = 0;
	while (cmdtext[i] != '\0' && p < sizeof (params) - 1)
		params[p++] = cmdtext[i++];
	params[p] = '\0';

	// resolve an alias to its target command name
	new h = XCmd_Hash(name);
	new ai = XCmd_FindAlias(h);
	if (ai != -1)
	{
		name = "";
		strcat(name, XCmd_AliasTo[ai]);
		h = XCmd_Hash(name);
	}

	new fn[5 + 32] = "xcmd_";
	strcat(fn, name);

	new e_COMMAND_ERRORS:result;
	if (funcidx(fn) < 0)
		result = COMMAND_UNDEFINED;
	else if (Command_GetPlayerDisabled(playerid) || sethas(XCmd_Disabled, h))
		result = COMMAND_DISABLED;
	else
	{
		// pre-hook: undefined callback -> 0 (allow); an error (>= 2) vetoes
		new e_COMMAND_ERRORS:pre =
			e_COMMAND_ERRORS:CallLocalFunction("OnPlayerCommandReceived", "isi", playerid, cmdtext, _:COMMAND_OK);
		if (_:pre >= _:COMMAND_UNDEFINED)
			result = pre;
		else
			result = e_COMMAND_ERRORS:CallLocalFunction(fn, "isi", playerid, params, 0);
	}
	// post-hook: OnPlayerCommandPerformed(playerid, cmdtext, result) may rewrite
	// the result (an undefined callback returns 0, meaning "no change").
	new e_COMMAND_ERRORS:post =
		e_COMMAND_ERRORS:CallLocalFunction("OnPlayerCommandPerformed", "isi", playerid, cmdtext, _:result);
	if (_:post != 0)
		result = post;

	// map to the OnPlayerCommandText return: 1 suppresses SA:MP's "unknown
	// command" message, 0 lets it show. A known-but-blocked command is
	// suppressed; a truly unknown one shows the message unless turned off.
	switch (result)
	{
		case COMMAND_OK, COMMAND_SILENT, COMMAND_DENIED, COMMAND_DISABLED, COMMAND_HIDDEN, COMMAND_NO_PLAYER, COMMAND_INVALID_INPUT:
			return HOOK_STOP_1;
		case COMMAND_UNDEFINED, COMMAND_BAD_PREFIX:
			return XCmd_ShowUnknown ? HOOK_CONTINUE_0 : HOOK_STOP_1;
		default:
			return HOOK_CONTINUE_0;          // COMMAND_ZERO_RET etc: not handled
	}
}



