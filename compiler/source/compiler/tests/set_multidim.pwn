#include <console>
#include <foreach>

new veh[3][8];   // players-per-vehicle: 3 vehicles, each an independent set
new active[4];   // the set of vehicle ids that currently have players

main()
{
	for (new v = 0; v < 3; v++)
		setinit(veh[v]);
	setinit(active);

	new v1 = 1, v2 = 2;              // runtime (non-constant) row indices
	setadd(veh[v1], 5);
	setadd(veh[v1], 2);
	setadd(veh[v1], 9);
	setadd(veh[v2], 7);
	setadd(active, v1);
	setadd(active, v2);

	// nested foreach: for each active vehicle walk its player set,
	// the inner operand indexed by the OUTER loop variable
	foreach (new v : active)
	{
		printf("veh %d len %d\n", v, setlen(veh[v]));
		foreach (new p : veh[v])
			printf("  p %d\n", p);
	}

	// independence: removing from one row must not disturb another
	setremove(veh[v1], 5);
	printf("after len1=%d len2=%d has9=%d\n",
		setlen(veh[v1]), setlen(veh[v2]), sethas(veh[v1], 9));
}
