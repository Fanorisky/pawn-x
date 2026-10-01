#include <console>
#include <foreach>

// A lifted local (generator/async) does NOT increment "declared" (its slot is in
// the state block), so scope cleanup keyed on "declared" left its symbol in
// "loctab": a following sibling scope reusing the name then errored 21 ("symbol
// already defined"). Here two for-loops AND two set-walks reuse "i"/"v".
iterfunc Two()
{
	for (new i = 0; i < 2; i++)
		yield return i;
	for (new i = 0; i < 2; i++)
		yield return i + 10;
}

new gs[8];
iterfunc Walks()
{
	setadd(gs, 1);
	setadd(gs, 2);
	foreach (new v : gs)
		yield return v;
	foreach (new v : gs)
		yield return v * 10;
}

main()
{
	new n = 0, sum = 0;
	foreach (new a : Two()) { sum += a; n++; }
	printf("two n=%d sum=%d\n", n, sum);      // 4, 0+1+10+11 = 22
	sum = 0; n = 0;
	foreach (new b : Walks()) { sum += b; n++; }
	printf("walks n=%d sum=%d\n", n, sum);    // 4, 1+2+10+20 = 33
}
