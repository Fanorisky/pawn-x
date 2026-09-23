#if defined _INC_actors
	#endinput
#endif
#define _INC_actors

/**
 * <library name="actors">
 *  Tracked-actor iterator — the same wrapper pattern as <vehicles>. Actors are
 *  native-driven (CreateActor/DestroyActor); pawn-x does not hook native calls,
 *  so Actor_Create/Actor_Destroy maintain an `Actor` set as they call the
 *  native:
 *
 *    new a = Actor_Create(skin, x, y, z, angle);
 *    foreach (new id : Actor) { ... }
 *    Actor_Destroy(a);
 *
 *  LIMITATION: only actors created through Actor_Create are tracked. Needs the
 *  `iterset` companion plugin for the set* natives.
 * </library>
 */

#include <foreach>

#if !defined MAX_ACTORS
	#define MAX_ACTORS (1000)
#endif
#if !defined INVALID_ACTOR_ID
	#define INVALID_ACTOR_ID (65535)
#endif

new Actor[MAX_ACTORS + 1];

stock Actor_Create(skin, Float:x, Float:y, Float:z, Float:angle)
{
	new id = CreateActor(skin, x, y, z, angle);
	if (id != INVALID_ACTOR_ID)
		setadd(Actor, id);
	return id;
}

stock Actor_Destroy(actorid)
{
	setremove(Actor, actorid);
	return DestroyActor(actorid);
}
