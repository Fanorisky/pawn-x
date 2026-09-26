#include <console>
#include <async>

// An UNSIZED array parameter of an "async" coroutine is rejected (error 268): with
// no compile-time cell count, its cells cannot be copied into the coroutine's state
// block, and the caller's array is gone after the first suspend. Give the parameter
// a fixed size ("buf[64]") so it can be copied in (see async_array_param), or read it
// before the first await. Multi-dimensional and by-"&"reference parameters are
// rejected for the same reason.

async F(const buf[])                 // unsized: no size to copy in -> error 268
{
    new x = await 0;
    return x + buf[0];
}

main() { new a[2] = {1, 2}; new t = Async_Start(F, a); Async_Resume(t, 5); }
