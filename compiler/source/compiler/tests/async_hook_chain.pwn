#include <console>
#include <async>
#include <hook>

// Repro of the Harwana/Mode startup crash: a callback that is BOTH declared as a
// user "public" (the chain tail) and hooked by a plain "hook" AND an "async hook".
// Mode's OnStartSQL has exactly this shape (gamemode.pwn declares
// `forward OnStartSQL(); public OnStartSQL()`, four other files add `hook` bodies,
// two of them `async hook`), and calling it crashed the host with EIP=0.

forward OnStartSQL();
hook OnStartSQL() { printf("plain-hook\n"); return 1; }
async hook OnStartSQL() { printf("async-hook-start\n"); new v = await 0; printf("async-hook-resumed %d\n", v); return 1; }
public OnStartSQL() { printf("user-public\n"); return 1; }

main()
{
	new r = OnStartSQL();
	printf("r=%d\n", r);
}
