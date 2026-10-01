#if defined _INC_x_groups
	#endinput
#endif
#define _INC_x_groups

/*
 * x_groups - player permission groups for PawnX, a drop-in for YSI's y_groups.
 *
 * A group is a set of players (teams, admin levels, factions). Each group
 * holds a tri-state permission (ALLOW / DENY / UNDEF, tag E_GROUP_SET) over an
 * entity, which is any integer id the caller assigns. A command id counts as
 * an entity, which is why YCMD:name works directly as the entity argument to
 * the Group_Command family below.
 *
 * Everyone is implicitly in GROUP_GLOBAL, whose per-entity default is ALLOW,
 * so everything is permitted until you lock it down. Resolution matches
 * y_groups: a DENY in any of the player's groups (or the global group) blocks
 * the entity, otherwise a single ALLOW permits it, otherwise the global
 * default decides. The model is plain Pawn, no plugin and no host native.
 *
 * Existing y_groups code compiles by swapping the include:
 *   #include <YSI_Players\y_groups>  ->  #include <x_groups>
 *
 * Not matched: the per-library Group_Set<Lib>... metaprogramming, gangs,
 * colours, and balanced groups. Define X_GROUPS_STANDALONE to drop the
 * OnPlayerDisconnect cleanup hook (used by the disk-free test).
 */

#include <string>

#if !defined MAX_GROUPS
	#define MAX_GROUPS (32)
#endif
#if !defined MAX_GROUP_NAME
	#define MAX_GROUP_NAME (24)
#endif
#if !defined X_GROUPS_MAX_PLAYERS
	#if defined MAX_PLAYERS
		#define X_GROUPS_MAX_PLAYERS MAX_PLAYERS
	#else
		#define X_GROUPS_MAX_PLAYERS (1000)
	#endif
#endif
#if !defined X_GROUPS_MAX_ENTITIES
	#define X_GROUPS_MAX_ENTITIES (256)
#endif

#define X_GROUPS_PLAYER_CELLS (((X_GROUPS_MAX_PLAYERS) + 31) / 32)

// Predefined group everyone belongs to, and the "no such group" sentinel.
#define GROUP_GLOBAL  (Group:0)
#define INVALID_GROUP (Group:-1)

// Tri-state permission (same names and tag as y_groups).
enum E_GROUP_SET
{
	DENY      = 0x80000000,
	UNDEF     = 0,
	UNDEFINED = 0,
	ALLOW     = 1
}

static bool:gGroupUsed[MAX_GROUPS];  // slot 0 is the global group, handled specially
static gGroupName[MAX_GROUPS][MAX_GROUP_NAME];
static gGroupMembers[MAX_GROUPS][X_GROUPS_PLAYER_CELLS];
static E_GROUP_SET:gGroupPerm[MAX_GROUPS][X_GROUPS_MAX_ENTITIES];
static E_GROUP_SET:gGroupGlobalDefault = ALLOW;

// --- internal helpers ---

static stock bool:_XG_InRange(Group:g)
{
	if (g == GROUP_GLOBAL)
		return true;
	return 0 < _:g < MAX_GROUPS && gGroupUsed[_:g];
}

static stock bool:_XG_EntityOk(entity)
{
	return 0 <= entity < X_GROUPS_MAX_ENTITIES;
}

static stock bool:_XG_PlayerOk(playerid)
{
	return 0 <= playerid < X_GROUPS_MAX_PLAYERS;
}

// --- lifecycle ---

stock Group:Group_Create(const name[] = "")
{
	for (new i = 1; i < MAX_GROUPS; ++i)
	{
		if (gGroupUsed[i])
			continue;
		gGroupUsed[i] = true;
		strpack(gGroupName[i], name, sizeof gGroupName[]);
		for (new c = 0; c < X_GROUPS_PLAYER_CELLS; ++c)
			gGroupMembers[i][c] = 0;
		for (new e = 0; e < X_GROUPS_MAX_ENTITIES; ++e)
			gGroupPerm[i][e] = UNDEF;
		return Group:i;
	}
	return INVALID_GROUP;
}

stock bool:Group_IsValid(Group:g)
{
	return _XG_InRange(g);
}

stock Group_Destroy(Group:g)
{
	if (g == GROUP_GLOBAL || !_XG_InRange(g))
		return;
	gGroupUsed[_:g] = false;
	gGroupName[_:g][0] = '\0';
}

stock Group:Group_GetID(const name[])
{
	for (new i = 1; i < MAX_GROUPS; ++i)
		if (gGroupUsed[i] && !strcmp(gGroupName[i], name, true))
			return Group:i;
	return INVALID_GROUP;
}

stock Group_GetName(Group:g, dest[], size = sizeof dest)
{
	if (!_XG_InRange(g))
		return dest[0] = '\0', 0;
	strunpack(dest, gGroupName[_:g], size);
	return strlen(dest);
}

stock Group_SetName(Group:g, const name[])
{
	if (_XG_InRange(g) && g != GROUP_GLOBAL)
		strpack(gGroupName[_:g], name, sizeof gGroupName[]);
}

stock Group_GetCount(Group:g)
{
	if (!_XG_InRange(g) || g == GROUP_GLOBAL)
		return 0;
	new total = 0;
	for (new c = 0; c < X_GROUPS_PLAYER_CELLS; ++c)
	{
		new bits = gGroupMembers[_:g][c];
		while (bits)
		{
			bits &= bits - 1;
			++total;
		}
	}
	return total;
}

// --- membership ---

stock Group_SetPlayer(Group:g, playerid, bool:member)
{
	// Global membership is implicit; you cannot add to or leave it here.
	if (g == GROUP_GLOBAL || !_XG_InRange(g) || !_XG_PlayerOk(playerid))
		return;
	new cell = playerid >>> 5, mask = 1 << (playerid & 31);
	if (member)
		gGroupMembers[_:g][cell] |= mask;
	else
		gGroupMembers[_:g][cell] &= ~mask;
}

stock bool:Group_GetPlayer(Group:g, playerid)
{
	if (!_XG_PlayerOk(playerid))
		return false;
	if (g == GROUP_GLOBAL)
		return true;
	if (!_XG_InRange(g))
		return false;
	return (gGroupMembers[_:g][playerid >>> 5] & (1 << (playerid & 31))) != 0;
}

// Drop a player from every created group. Call this when a slot is freed.
stock Group_ResetPlayer(playerid)
{
	if (!_XG_PlayerOk(playerid))
		return;
	new cell = playerid >>> 5, mask = ~(1 << (playerid & 31));
	for (new i = 1; i < MAX_GROUPS; ++i)
		gGroupMembers[i][cell] &= mask;
}

// --- permissions (generic entity API) ---

stock Group_SetEntity(Group:g, entity, E_GROUP_SET:perm)
{
	if (_XG_InRange(g) && _XG_EntityOk(entity))
		gGroupPerm[_:g][entity] = perm;
}

stock E_GROUP_SET:Group_GetEntity(Group:g, entity)
{
	if (_XG_InRange(g) && _XG_EntityOk(entity))
		return gGroupPerm[_:g][entity];
	return UNDEF;
}

// The fallback for the global group when an entity has no explicit global
// entry. Defaults to ALLOW (everything permitted by default).
stock Group_SetGlobalDefault(E_GROUP_SET:perm)
{
	gGroupGlobalDefault = perm;
}

// --- resolution (DENY wins; else one explicit ALLOW; else the default) ---

stock bool:Group_PlayerAllowed(playerid, entity)
{
	if (!_XG_PlayerOk(playerid) || !_XG_EntityOk(entity))
		return false;
	new bool:anyAllow = false;
	for (new i = 1; i < MAX_GROUPS; ++i)
	{
		if (!gGroupUsed[i] || !Group_GetPlayer(Group:i, playerid))
			continue;
		new E_GROUP_SET:pm = gGroupPerm[i][entity];
		if (pm == DENY)
			return false;
		if (pm == ALLOW)
			anyAllow = true;
	}
	new E_GROUP_SET:gp = gGroupPerm[0][entity];
	if (gp == DENY)
		return false;
	if (gp == ALLOW)
		anyAllow = true;
	if (anyAllow)
		return true;
	return gGroupGlobalDefault == ALLOW;
}

stock bool:Group_PlayerDenied(playerid, entity)
{
	if (!_XG_PlayerOk(playerid) || !_XG_EntityOk(entity))
		return false;
	for (new i = 1; i < MAX_GROUPS; ++i)
	{
		if (gGroupUsed[i] && Group_GetPlayer(Group:i, playerid) &&
			gGroupPerm[i][entity] == DENY)
			return true;
	}
	return gGroupPerm[0][entity] == DENY;
}

// --- command entity family (drop-in y_commands integration) ---
// A command id is just an entity, so these are thin names over the generic
// API. YCMD:name resolves to the command id you pass here.

#define Group_SetCommand(%0,%1,%2)       Group_SetEntity((%0), (%1), (%2))
#define Group_GetCommand(%0,%1)          Group_GetEntity((%0), (%1))
#define Group_CommandAllowed(%0,%1)      (Group_GetEntity((%0), (%1)) == ALLOW)
#define Group_CommandDenied(%0,%1)       (Group_GetEntity((%0), (%1)) == DENY)
#define Group_SetGlobalCommand(%0,%1)    Group_SetEntity(GROUP_GLOBAL, (%0), (%1))
#define Group_GetGlobalCommand(%0)       Group_GetEntity(GROUP_GLOBAL, (%0))
#define Group_GlobalCommandAllowed(%0)   (Group_GetEntity(GROUP_GLOBAL, (%0)) == ALLOW)
#define Group_GlobalCommandDenied(%0)    (Group_GetEntity(GROUP_GLOBAL, (%0)) == DENY)
#define Group_SetGlobalCommandDefault(%0) Group_SetGlobalDefault(%0)

#if !defined X_GROUPS_STANDALONE
#include <hook>

// Free a disconnecting player's group membership so a reused slot starts clean.
hook OnPlayerDisconnect(playerid, reason)
{
	#pragma unused reason
	Group_ResetPlayer(playerid);
	return 1;
}
#endif
