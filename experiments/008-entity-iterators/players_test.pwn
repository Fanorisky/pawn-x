#include <open.mp>
#include <players>

main()
{
	print("[P] connect 5,2,9");
	CallLocalFunction("OnPlayerConnect", "i", 5);
	CallLocalFunction("OnPlayerConnect", "i", 2);
	CallLocalFunction("OnPlayerConnect", "i", 9);
	print("[P] foreach Player:");
	foreach (new i : Player) printf("  %d", i);
	print("[P] disconnect 5");
	CallLocalFunction("OnPlayerDisconnect", "ii", 5, 0);
	print("[P] foreach Player:");
	foreach (new i : Player) printf("  %d", i);
	printf("[P] len=%d", setlen(Player));
	printf("[DONE] players");
}
