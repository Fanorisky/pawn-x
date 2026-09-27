#include <hash>

// hash() takes exactly one operand. A string literal followed by anything
// other than ')' (a stray extra argument) must be a clean arity error, not a
// silent miscompile. Regression guard for the redundant-lexpush bug.
main()
{
    new x = hash("gun", 5);
    #pragma unused x
}
