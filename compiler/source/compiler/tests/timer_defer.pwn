#include <console>

// Native one-shot timers: "timer Name[interval](params)" declares a public
// invoked by name, and "defer Name[ms](args)" schedules a single call, lowering
// to Timer_SetEx("Name", ms, false, "<fmt>", args) with <fmt> derived from the
// argument tags. A format native takes its variadic args BY REFERENCE, so a
// scalar is passed as the address of a cell holding it; Timer_SetEx (here a stub
// that reads them with getarg) dereferences per <fmt>. This is the YSI y_timers
// timer/defer replacement, plugin-free.

stock Timer_SetEx(const func[], interval, bool:repeat, const fmt[], {Float,_}:...)
{
	new a0 = getarg(4), a1 = getarg(5);
	printf("timer=%s ms=%d repeat=%d fmt=%s args=%d,%d", func, interval, repeat, fmt, a0, a1);
	return 0;
}

// the [1000] is the DEFAULT interval used by a bare "defer Greet(...)"
timer Greet[1000](playerid, count)
{
	printf("greet %d %d", playerid, count);
}

main()
{
	defer Greet[500](7, 3);   // explicit interval -> ms=500, fmt "ii", args 7,3
	defer Greet(9, 1);        // default interval  -> ms=1000, args 9,1
}
