#include <console>
#include <async>

/* MID-EXPRESSION await with a live operand temporary is rejected (error 099).
 * In "base + await 0" the operator pushes "base" onto the stack before the await
 * evaluates; that temporary would be discarded by the suspend's frame unwind, so
 * the other operand would read back as garbage. Previously this SILENTLY
 * miscompiled (async lifts all locals, so the "declared" guard never saw the
 * operator temp); now pc_exprtemp catches the live temporary and rejects cleanly.
 * Put the await in statement position instead: "new v = await 0; new x = base + v;". */

async F()
{
    new base = 5;
    new x = base + await 0;       // error 099: live temporary across the suspend
    printf("%d\n", x);
    return x;
}

main() { new t = Async_Start(F); Async_Resume(t, 100); }
