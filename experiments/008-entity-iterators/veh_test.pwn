#include <open.mp>
#include <vehicles>

main()
{
	new a = Vehicle_Create(400, 0.0, 0.0, 3.0, 0.0, -1, -1, 60);
	new b = Vehicle_Create(401, 5.0, 0.0, 3.0, 0.0, -1, -1, 60);
	new c = Vehicle_Create(402, 10.0, 0.0, 3.0, 0.0, -1, -1, 60);
	printf("[V] created %d %d %d len=%d", a, b, c, setlen(Vehicle));
	print("[V] foreach:");
	foreach (new id : Vehicle) printf("  %d", id);
	Vehicle_Destroy(b);
	printf("[V] after destroy %d, len=%d has=%d", b, setlen(Vehicle), sethas(Vehicle, b));
	foreach (new id : Vehicle) printf("  %d", id);
	printf("[DONE] veh");
}
