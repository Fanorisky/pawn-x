#include <console>

// Regression: the AMX switch binary-search compared cases with a subtraction
// (case - pri), which overflows when the case value and the switched value
// straddle more than the signed cell range. A case label of cellmin
// (0x80000000), or any table spanning the full range, could then be missed.

sw(x)
{
	switch (x)
	{
		case cellmin:     return 100;
		case -1000000000: return 101;
		case 0:           return 102;
		case 1000000000:  return 103;
		case cellmax:     return 104;
	}
	return -1;
}

main()
{
	printf("min=%d\n", sw(cellmin));
	printf("neg=%d\n", sw(-1000000000));
	printf("zero=%d\n", sw(0));
	printf("pos=%d\n", sw(1000000000));
	printf("max=%d\n", sw(cellmax));
	printf("def=%d\n", sw(5));
}
