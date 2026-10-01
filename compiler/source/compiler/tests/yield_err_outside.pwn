#include <console>
#pragma pawnx_yield   // enable the "yield" keyword without pulling in <foreach>, so the misuse below is diagnosed
func()
{
	yield return 1;
}
main() { func(); }
