#include <console>
#include <x_ini>

// Drop-in y_ini syntax: an INI: callback plus the in-callback readers. This
// calls the callback directly with (key, value) pairs (no disk, no
// CallLocalFunction) to prove the reader macros parse each type.

new gScore, gColor, gFlags, gName[24];
new bool:gAdmin;

INI:save[Player](name[], value[])
{
	INI_Int("score", gScore);
	INI_Hex("color", gColor);
	INI_Bin("flags", gFlags);
	INI_Bool("admin", gAdmin);
	INI_String("name", gName);
	return 0;
}

Feed(key[], value[])
{
	@INI_save_Player(key, value);
}

main()
{
	new v1[] = "42", v2[] = "0xFF8800", v3[] = "1011", v4[] = "yes", v5[] = "Bob";
	new k1[] = "score", k2[] = "color", k3[] = "flags", k4[] = "admin", k5[] = "name";
	Feed(k1, v1);
	Feed(k2, v2);
	Feed(k3, v3);
	Feed(k4, v4);
	Feed(k5, v5);
	printf("score=%d\n", gScore);
	printf("color=%d\n", gColor);
	printf("flags=%d\n", gFlags);
	printf("admin=%d\n", _:gAdmin);
	printf("name=%s\n", gName);
}
