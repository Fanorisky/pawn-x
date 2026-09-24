#include <console>
#include <foreach>

iterfunc Classify(n)
{
	for (new i = 0; i != n; ++i)
	{
		if (i % 3 == 0)
		{
			yield return i;
		}
		else if (i % 3 == 1)
		{
			yield return i * 10;
		}
		else
		{
			if (i > 5) return;      // early end of sequence
			yield return -i;
		}
	}
}

main()
{
	foreach (new v : Classify(9)) printf("b %d\n", v);
	printf("done\n");
}
