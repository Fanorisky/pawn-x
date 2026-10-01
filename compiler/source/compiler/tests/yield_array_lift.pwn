#include <console>
#include <foreach>

// A fixed-size local array can now span a "yield": it is lifted into the
// generator state block (no stack snapshot). Covers a plain array, an array
// whose cells accumulate across yields, and a 2-D array.
iterfunc Emit()
{
	new buf[3];
	buf[0] = 10;
	buf[1] = 20;
	buf[2] = 30;
	for (new i = 0; i < 3; i++)
		yield return buf[i];
}
iterfunc Accumulate()
{
	new acc[4];
	acc[0] = 1;
	for (new i = 1; i < 4; i++)
	{
		acc[i] = acc[i - 1] * 2;
		yield return acc[i];
	}
}
iterfunc Grid()
{
	new g[2][2];
	g[0][0] = 1; g[0][1] = 2; g[1][0] = 3; g[1][1] = 4;
	for (new i = 0; i < 2; i++)
		for (new j = 0; j < 2; j++)
			yield return g[i][j];
}

main()
{
	new a = 0;
	foreach (new v : Emit()) a += v;
	printf("emit=%d\n", a);
	new b = 0;
	foreach (new v : Accumulate()) b += v;
	printf("acc=%d\n", b);
	new c = 0;
	foreach (new v : Grid()) c += v;
	printf("grid=%d\n", c);
}
