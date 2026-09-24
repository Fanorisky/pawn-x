#include <console>
#include <foreach>
iterfunc Bad()
{
	new x = yield return 1;
}
main() { foreach (new v : Bad()) printf("%d\n", v); }
