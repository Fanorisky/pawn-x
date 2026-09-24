#include <console>
#include <foreach>

// A generator whose loop variable "i" is a lifted local: it must survive
// across each "yield" suspend so the loop advances 0, 1, 2.
iterfunc Count()
{
	for (new i = 0; i != 3; ++i)
		yield return i;
}

main()
{
	foreach (new v : Count()) printf("v %d\n", v);
	printf("done\n");
}
