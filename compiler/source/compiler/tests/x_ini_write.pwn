#include <console>
#include <x_ini>

// Compile-only: the buffered write API maps onto the host config natives
// (writecfg/writecfgvalue/deletecfg), which the test runner does not provide,
// so this only proves the surface builds clean under the strict flags.

main()
{
	new INI:f = INI_Open("player.ini");
	if (f == INI_NO_FILE)
		return;
	INI_SetTag(f, "data");
	INI_WriteString(f, "name", "Bob");
	INI_WriteInt(f, "score", 42);
	INI_WriteFloat(f, "health", 99.5, 2);
	INI_WriteBool(f, "admin", true);
	INI_WriteHex(f, "color", 0xFF8800);
	INI_WriteBin(f, "flags", 11);
	INI_RemoveEntry(f, "score");
	INI_DeleteTag(f, "old");
	INI_Close(f);
}
