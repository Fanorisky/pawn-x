#define X_COMMANDS_STANDALONE
#include <console>
#include <x_commands>

// Drop-in YCMD: syntax. Standalone (no dispatcher hook) so it runs without
// the host: proves the dual-role macro (definition + id expression), the
// registry, and the Command_* state API.

YCMD:heal(playerid, params[], help)
{
	#pragma unused playerid, params, help
	return _:COMMAND_OK;
}

YCMD:kick(playerid, params[], help)
{
	#pragma unused playerid, params, help
	return _:COMMAND_OK;
}

main()
{
	printf("heal=%d\n", YCMD:heal);
	printf("kick=%d\n", YCMD:kick);
	printf("heal2=%d\n", YCMD:heal);
	printf("valid=%d\n", _:Command_IsValid(YCMD:heal));
	Command_SetDisabled(YCMD:heal);
	printf("dis=%d\n", _:Command_GetDisabled(YCMD:heal));
	printf("diskick=%d\n", _:Command_GetDisabled(YCMD:kick));
	Command_SetHidden(YCMD:kick, true);
	printf("hid=%d\n", _:Command_GetHidden(YCMD:kick));
	Command_SetPlayerDisabled(3, true);
	printf("poff=%d\n", _:Command_GetPlayerDisabled(3));
	Command_AddAltNamed("heal", "h");
	new nm[32];
	Command_GetName(YCMD:heal, nm);
	printf("name=%s\n", nm);
	printf("unk=%d\n", _:Command_GetUnknownReturn());
}
