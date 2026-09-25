#include <console>
#include <hook>

/* A CALLBACK hook whose target is literally named "function". "function" is a
 * contextual modifier for call-site hooks ("hook function Name(...)"), so the
 * modifier disambiguation must not clobber the callback name when no target
 * name follows -- here "function" is the callback itself. */
hook function(a, b)
{
    printf("sum %d\n", a + b);
    return HOOK_CONTINUE;
}

main()
{
    function(4, 5);
    printf("done\n");
}
