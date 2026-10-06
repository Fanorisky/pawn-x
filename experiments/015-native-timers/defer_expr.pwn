// Isolation: defer with an EXPRESSION argument (n+1), driven once.
// Prints the received value each step; correct = 1,2,3 then stop.
#include <open.mp>
#include <foreach>
#include <players>
#include <timers>
#include <hook>

main() {}

timer Step[300](n)
{
    printf("[X] step received n=%d", n);
    if (n < 3)
    {
        new next = n + 1;
        printf("[X]   scheduling next=%d", next);
        defer Step[300](next);        // plain variable arg
    }
}

timer Step2[300](n)
{
    printf("[Y] step2 received n=%d", n);
    if (n < 3)
        defer Step2[300](n + 1);      // inline expression arg
}

hook OnGameModeInit()
{
    defer Step[300](1);
    defer Step2[300](1);
    return 1;
}
