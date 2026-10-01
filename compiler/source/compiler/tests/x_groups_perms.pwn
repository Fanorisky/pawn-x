#define X_GROUPS_STANDALONE
#define X_GROUPS_MAX_PLAYERS (64)
#include <console>
#include <x_groups>

// Drop-in y_groups names: Group: tag, ALLOW/DENY/UNDEF, and the Group_Command
// family. Checks that DENY wins, an explicit ALLOW beats a deny-all default,
// and the global default decides when nothing is explicit.

main()
{
	new Group:mods = Group_Create("mods");
	new Group:jail = Group_Create("jail");
	new p = 5, ent = 7;

	Group_SetGlobalDefault(ALLOW);
	printf("d1=%d\n", _:Group_PlayerAllowed(p, ent));

	Group_SetGlobalDefault(DENY);
	printf("d2=%d\n", _:Group_PlayerAllowed(p, ent));

	Group_SetPlayer(mods, p, true);
	Group_SetCommand(mods, ent, ALLOW);
	printf("d3=%d\n", _:Group_PlayerAllowed(p, ent));
	printf("ca=%d\n", _:Group_CommandAllowed(mods, ent));

	Group_SetPlayer(jail, p, true);
	Group_SetCommand(jail, ent, DENY);
	printf("d4=%d\n", _:Group_PlayerAllowed(p, ent));
	printf("d5=%d\n", _:Group_PlayerDenied(p, ent));

	Group_SetPlayer(jail, p, false);
	printf("d6=%d\n", _:Group_PlayerAllowed(p, ent));

	new q = 9;
	printf("d7=%d\n", _:Group_PlayerAllowed(q, ent));
	Group_SetGlobalCommand(ent, ALLOW);
	printf("d8=%d\n", _:Group_PlayerAllowed(q, ent));
	Group_SetGlobalCommand(ent, DENY);
	printf("d9=%d\n", _:Group_PlayerAllowed(p, ent));

	printf("cnt=%d\n", Group_GetCount(mods));
	printf("id=%d\n", _:Group_GetID("jail"));
	printf("glob=%d\n", _:Group_GetPlayer(GROUP_GLOBAL, q));
}
