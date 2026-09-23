#if defined _INC_vehicles
	#endinput
#endif
#define _INC_vehicles

/**
 * <library name="vehicles">
 *  Tracked-vehicle iterator. A vehicle's lifecycle is native-driven
 *  (CreateVehicle/DestroyVehicle) and pawn-x does not hook native call sites, so
 *  instead of auto-tracking every vehicle it provides thin wrappers that add to
 *  / remove from a `Vehicle` set as they call the native:
 *
 *    new v = Vehicle_Create(model, x, y, z, a, c1, c2, respawn);
 *    foreach (new id : Vehicle) { ... }
 *    Vehicle_Destroy(v);
 *
 *  LIMITATION: only vehicles created through Vehicle_Create are tracked —
 *  vehicles made by a raw CreateVehicle call or by another script are not in the
 *  set. (True auto-tracking would need native-call interception; out of scope.)
 *  Needs the `iterset` companion plugin for the set* natives.
 * </library>
 */

#include <foreach>

#if !defined MAX_VEHICLES
	#define MAX_VEHICLES (2000)
#endif
#if !defined INVALID_VEHICLE_ID
	#define INVALID_VEHICLE_ID (65535)
#endif

new Vehicle[MAX_VEHICLES + 1];

stock Vehicle_Create(modelid, Float:spawnX, Float:spawnY, Float:spawnZ, Float:angle, colour1, colour2, respawnDelay, bool:addSiren = false)
{
	new id = CreateVehicle(modelid, spawnX, spawnY, spawnZ, angle, colour1, colour2, respawnDelay, addSiren);
	if (id != INVALID_VEHICLE_ID)
		setadd(Vehicle, id);
	return id;
}

stock Vehicle_Destroy(vehicleid)
{
	setremove(Vehicle, vehicleid);
	return DestroyVehicle(vehicleid);
}
