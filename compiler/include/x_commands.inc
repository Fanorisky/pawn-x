#if defined _INC_x_commands
	#endinput
#endif
#define _INC_x_commands

#include <hook>
#include <string>

/*
 * x_commands - commands for PawnX, a drop-in replacement for YSI's y_commands.
 *
 * The declarative syntax is the same as y_commands / ZCMD:
 *
 *     YCMD:heal(playerid, params[], help)
 *     {
 *         if (help) return COMMAND_OK;
 *         SetPlayerHealth(playerid, 100.0);
 *         return COMMAND_OK;
 *     }
 *
 * In expression position YCMD:heal resolves to the command id, so it drops
 * straight into x_groups: Group_SetCommand(g, YCMD:heal, ALLOW). CMD: and
 * COMMAND: are the ZCMD-style aliases (two params, no help). A hook on
 * OnPlayerCommandText parses the command, resolves aliases, checks the
 * per-command / per-player / group permission, runs OnPlayerCommandReceived,
 * dispatches, then runs OnPlayerCommandPerformed.
 *
 * Existing y_commands code compiles by swapping the include:
 *   #include <YSI_Coding\y_commands>  ->  #include <x_commands>
 * Include <x_groups> before this to gate commands by group permission.
 */

#if !defined MAX_COMMANDS
	#define MAX_COMMANDS (512)
#endif
#if !defined X_COMMANDS_MAX_PLAYERS
	#if defined MAX_PLAYERS
		#define X_COMMANDS_MAX_PLAYERS MAX_PLAYERS
	#else
		#define X_COMMANDS_MAX_PLAYERS (1000)
	#endif
#endif
#define COMMAND_NOT_FOUND (-1)

// Dispatch / return protocol, with y_commands' exact values.
enum e_COMMAND_ERRORS
{
	COMMAND_ZERO_RET      = 0,
	COMMAND_OK            = 1,
	COMMAND_UNDEFINED     = 2,
	COMMAND_DENIED        = 3,
	COMMAND_HIDDEN        = 4,
	COMMAND_NO_PLAYER     = 6,
	COMMAND_DISABLED      = 7,
	COMMAND_BAD_PREFIX    = 8,
	COMMAND_INVALID_INPUT = 10,
	COMMAND_SILENT        = 11
}

forward e_COMMAND_ERRORS:OnPlayerCommandReceived(playerid, cmdtext[], e_COMMAND_ERRORS:success);
forward e_COMMAND_ERRORS:OnPlayerCommandPerformed(playerid, cmdtext[], e_COMMAND_ERRORS:success);

// --- command registry: name (lowercased) -> small sequential id ---

static stock gCmdName[MAX_COMMANDS][32];
static stock gCmdCount = 0;
static stock bool:gCmdDisabled[MAX_COMMANDS];
static stock bool:gCmdHidden[MAX_COMMANDS];
static stock bool:gPlayerOff[X_COMMANDS_MAX_PLAYERS];
static stock gAliasName[MAX_COMMANDS][32];
static stock gAliasTo[MAX_COMMANDS];
static stock gAliasCount = 0;
static stock bool:gShowUnknown = true;

// Lowercase-copy a name (commands are case-insensitive).
static stock Cmd_Lower(dst[], const src[], len = sizeof dst)
{
	new i = 0;
	for ( ; src[i] != '\0' && i < len - 1; ++i)
	{
		new c = src[i];
		if (c >= 'A' && c <= 'Z')
			c += 'a' - 'A';
		dst[i] = c;
	}
	dst[i] = '\0';
}

// Look up a command id by name, assigning a new one if this is the first
// time the name is seen. This is what YCMD:name resolves to.
stock Command_GetID(const function[])
{
	new lo[32];
	Cmd_Lower(lo, function);
	for (new i = 0; i < gCmdCount; ++i)
		if (!strcmp(gCmdName[i], lo, true))
			return i;
	if (gCmdCount >= MAX_COMMANDS)
		return COMMAND_NOT_FOUND;
	strpack(gCmdName[gCmdCount], lo, 32);
	return gCmdCount++;
}

// --- declarative macros (dual-role YCMD, plus the ZCMD-style CMD / COMMAND) ---

// YCMD:name(playerid, params[], help) { } defines the handler public Cmd@name;
// YCMD:name in an expression resolves to Command_GetID("name").
#define _YCMD_0:(_:_YCMD_1:_YCMD_2:_YCMD_3:%0(%1[]%2) forward Cmd@%0(%1[]%2);public Cmd@%0(%1[]%2)
#define _YCMD_1:_YCMD_2:_YCMD_3:%0, Command_GetID(#%0)),
#define _YCMD_2:_YCMD_3:%0)  Command_GetID(#%0)))
#define _YCMD_3:%0;  Command_GetID(#%0));
#define YCMD: _YCMD_0:(_:_YCMD_1:_YCMD_2:_YCMD_3:

// ZCMD compatibility: CMD:name(playerid, params[]) with no help parameter.
#define CMD:%0(%1) forward Cmd@%0(%1, __help);public Cmd@%0(%1, __help) if (__help) return 0; else
#define COMMAND: CMD:

// --- command state API (ids come from Command_GetID / YCMD:name) ---

stock bool:Command_IsValid(cmd)
{
	return 0 <= cmd < gCmdCount;
}

stock Command_GetName(cmd, dest[], size = sizeof dest)
{
	if (!Command_IsValid(cmd))
		return dest[0] = '\0', 0;
	strunpack(dest, gCmdName[cmd], size);
	return strlen(dest);
}

stock Command_SetDisabled(cmd, bool:toggle = true)
{
	if (Command_IsValid(cmd))
		gCmdDisabled[cmd] = toggle;
}

stock bool:Command_GetDisabled(cmd)
{
	return Command_IsValid(cmd) && gCmdDisabled[cmd];
}

stock Command_SetHidden(cmd, bool:toggle = true)
{
	if (Command_IsValid(cmd))
		gCmdHidden[cmd] = toggle;
}

stock bool:Command_GetHidden(cmd)
{
	return Command_IsValid(cmd) && gCmdHidden[cmd];
}

stock Command_SetPlayerDisabled(playerid, bool:toggle = true)
{
	if (0 <= playerid < X_COMMANDS_MAX_PLAYERS)
		gPlayerOff[playerid] = toggle;
}

stock bool:Command_GetPlayerDisabled(playerid)
{
	return 0 <= playerid < X_COMMANDS_MAX_PLAYERS && gPlayerOff[playerid];
}

// Make altname run the handler of an existing command.
stock Command_AddAltNamed(const function[], const altname[])
{
	if (gAliasCount >= MAX_COMMANDS)
		return COMMAND_NOT_FOUND;
	Cmd_Lower(gAliasName[gAliasCount], altname);
	gAliasTo[gAliasCount] = Command_GetID(function);
	return gAliasCount++;
}

stock Command_SetUnknownReturn(bool:toggle)
{
	gShowUnknown = toggle;
}

stock bool:Command_GetUnknownReturn()
{
	return gShowUnknown;
}

// --- dispatcher (host: needs OnPlayerCommandText and CallLocalFunction) ---

#if !defined X_COMMANDS_STANDALONE
hook OnPlayerCommandText(playerid, cmdtext[])
{
	// Parse: optional '/', the command word (lowercased), then the tail.
	new i = 0;
	if (cmdtext[i] == '/')
		++i;
	new name[32], n = 0;
	while (cmdtext[i] > ' ' && n < sizeof (name) - 1)
	{
		new c = cmdtext[i];
		if (c >= 'A' && c <= 'Z')
			c += 'a' - 'A';
		name[n++] = c;
		++i;
	}
	name[n] = '\0';
	if (n == 0)
		return HOOK_CONTINUE_0;
	while (cmdtext[i] == ' ')
		++i;
	new params[128], p = 0;
	while (cmdtext[i] != '\0' && p < sizeof (params) - 1)
		params[p++] = cmdtext[i++];
	params[p] = '\0';

	// Resolve an alias to its target command name.
	for (new a = 0; a < gAliasCount; ++a)
	{
		if (!strcmp(gAliasName[a], name, false))
		{
			strunpack(name, gCmdName[gAliasTo[a]], sizeof name);
			break;
		}
	}

	new fn[4 + 32] = "Cmd@";
	strcat(fn, name);

	new e_COMMAND_ERRORS:result;
	if (funcidx(fn) < 0)
		result = COMMAND_UNDEFINED;
	else
	{
		new id = Command_GetID(name);
		if (Command_GetPlayerDisabled(playerid) || Command_GetDisabled(id))
			result = COMMAND_DISABLED;
#if defined _INC_x_groups
		else if (!Group_PlayerAllowed(playerid, id))
			result = COMMAND_DENIED;
#endif
		else
		{
			new e_COMMAND_ERRORS:pre =
				e_COMMAND_ERRORS:CallLocalFunction("OnPlayerCommandReceived", "isi", playerid, cmdtext, _:COMMAND_OK);
			if (_:pre >= _:COMMAND_UNDEFINED)
				result = pre;
			else
				result = e_COMMAND_ERRORS:CallLocalFunction(fn, "isi", playerid, params, 0);
		}
	}
	new e_COMMAND_ERRORS:post =
		e_COMMAND_ERRORS:CallLocalFunction("OnPlayerCommandPerformed", "isi", playerid, cmdtext, _:result);
	if (_:post != 0)
		result = post;

	switch (result)
	{
		case COMMAND_OK, COMMAND_SILENT, COMMAND_DENIED, COMMAND_DISABLED, COMMAND_HIDDEN, COMMAND_NO_PLAYER, COMMAND_INVALID_INPUT:
			return HOOK_STOP_1;
		case COMMAND_UNDEFINED, COMMAND_BAD_PREFIX:
			return gShowUnknown ? HOOK_CONTINUE_0 : HOOK_STOP_1;
		default:
			return HOOK_CONTINUE_0;
	}
}
#endif

