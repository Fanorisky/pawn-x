#include <console>
#include <foreach>

// A pawn-x set is one-dimensional by construction (array[0]=count, values in
// array[1..count]), so a multi-dimensional array can never be one. Iterating one
// as a set used to read row 0 as a count and then walk whatever the following
// cells held, which is silently wrong code. A multi-dimensional operand now walks
// its FIRST DIMENSION by index, matching YSI's "foreach (new i : twoDArray)" --
// the idiom for walking a 2D table of records (e.g. a per-vehicle data array).
new Vehicle[4][3];
new sets[2][8];      // the set-of-sets case: each row IS a one-dimensional set

main()
{
	Vehicle[0][0] = 11;
	Vehicle[3][2] = 99;

	/* index walk over the first dimension: 0,1,2,3 regardless of contents */
	foreach (new i : Vehicle)
		printf("v %d\n", i);

	/* Reverse walks it downwards */
	foreach (new j : Reverse(Vehicle))
		printf("r %d\n", j);

	/* a SUBSCRIPTED row is one-dimensional and keeps set semantics */
	setinit(sets[0]);
	setadd(sets[0], 42);
	setadd(sets[0], 7);
	foreach (new k : sets[0])
		printf("s %d\n", k);
}
