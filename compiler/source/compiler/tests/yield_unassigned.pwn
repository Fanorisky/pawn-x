#include <console>
#include <foreach>

// "flag" is read before ever being assigned; a zero-filled block means it is 0.
iterfunc Once()
{
	new flag;
	yield return flag;             // must print 0, not stack garbage
}

main()
{
	foreach (new v : Once()) printf("u %d\n", v);
	printf("done\n");
}
