#include <console>
#include <hook>

ComputeScore(playerid)
{
    printf("orig %d\n", playerid);
    return playerid * 10;
}

hook function ComputeScore(playerid)
{
    printf("pre %d\n", playerid);
    new r = continue(playerid);      // invoke the original
    printf("post %d\n", r);
    return r + 1;
}

main()
{
    new s = ComputeScore(5);         // redirected to the wrapper
    printf("result %d\n", s);
}
