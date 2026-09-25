#include <console>
#include <async>

// Three-level composition (experiment 012, task 2): A awaits B awaits C. Only
// the innermost coroutine (C) parks on an external "__pending()"; when main
// completes it, each "return" resumes its awaiter in turn via the return-to-
// awaiter path, and the value propagates all the way up to A. The "y * 10"
// argument exercises an ergonomic-await arg of the form <lifted local> OP <const>.
//   C(30): 30 + 100 = 130
//   B(3):  await C(3*10 = 30) -> 130, then 130 + 3 = 133
//   A:     await B(3) -> 133
async C(z)
{
    new v = await __pending();        // the only real suspension in the chain
    return z + v;
}

async B(y)
{
    new r = await C(y * 10);          // ergonomic await of an async fn, arg = y*10
    return r + 3;
}

async A()
{
    new r = await B(3);
    printf("chain final=%d\n", r);
}

main()
{
    Async_Start(A);                   // A -> B -> C all park on C's __pending()
    printf("(chain parked)\n");
    Async_ResumeInner(100);           // complete C; 130 -> 133 unwinds up to A
}
