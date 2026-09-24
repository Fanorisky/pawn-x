#include <console>
#include <foreach>
iterfunc Bad(&acc, cur)
{
	yield return 1;
}
main() { foreach (new v : Bad()) printf("%d\n", v); }
