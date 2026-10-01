#include <console>

// Using the foreach keyword without <foreach> must give a clear error, not a
// cryptic "undefined symbol setget/setnext" (Y-Less issue #1 follow-up).
main()
{
	new data[20];
	foreach (new i : data)
	{
		printf("%d", i);
	}
}
