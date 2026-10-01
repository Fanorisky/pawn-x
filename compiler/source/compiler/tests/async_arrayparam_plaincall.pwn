#include <console>
#include <async>

// An "async" function's parameters shift up one cell: the caller pushes the
// coroutine's state block as arg0, so the user's first argument lands where an
// ordinary call would put its second. A LIFTED scalar accounts for that; an
// UNLIFTED parameter (an unsized array, a multi-dimensional array or a "&"
// reference, none of which can be copied into the block) did not, so the body
// read the state block in place of the parameter. For an array parameter that
// meant reading the zero-filled block as a string, i.e. always "".
// In Harwana/Mode this showed up as four "SELECT COUNT(*) FROM ``" errors from
// Main_CountRows("properties") and friends.
async Show(const name[], tag)
{
	printf("name=%s tag=%d\n", name, tag);
}

main()
{
	Show("hello", 42);
	printf("after\n");
}
