#include <console>

// "repeat Name[ms](args)" starts a REPEATING timer and RETURNS its handle (an
// expression, so it can be stored); "stop <handle>" kills it. Together with
// timer/defer these are the full YSI y_timers family, native and plugin-free:
// repeat lowers to Timer_SetEx(...,true,...) leaving the handle in PRI, stop to
// Timer_Kill(handle). Timer_SetEx/Timer_Kill are stubs here so pawnruns can run it.

stock Timer_SetEx(const func[], interval, bool:repeat, const fmt[], {Float,_}:...)
{
	printf("set %s ms=%d repeat=%d fmt=%s a=%d", func, interval, repeat, fmt, getarg(4));
	return 12345;   // fake handle
}
stock Timer_Kill(timerid)
{
	printf("kill %d", timerid);
	return 1;
}

timer Tick[250](playerid)
{
	printf("tick %d", playerid);
}

main()
{
	new h = repeat Tick(7);       // default interval 250, handle -> h
	printf("h=%d", h);
	stop h;                        // Timer_Kill(h)

	new h2 = repeat Tick[500](9); // explicit interval 500
	printf("h2=%d", h2);
	stop h2;
}
