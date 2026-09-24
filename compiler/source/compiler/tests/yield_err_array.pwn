#include <console>
#include <foreach>
iterfunc Bad()
{
	new arr[4];
	yield return arr[0];
}
main() { foreach (new v : Bad()) printf("%d\n", v); }
