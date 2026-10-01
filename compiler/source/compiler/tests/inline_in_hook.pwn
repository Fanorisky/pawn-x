#include <console>

// Regression: an "inline" defined inside a hook body used to fail because the
// inline's hidden name embedded the hook's long mangled name and overflowed
// sNAMEMAX (warning 200 -> undefined symbol). The name is now hashed, so it
// fits regardless of the enclosing function's name.
Apply(const arr[], size, Callback:cb)
{
	for (new i = 0; i < size; i++)
		cb(arr[i]);
}

forward Ev();

hook Ev()
{
	new sum = 0;
	inline Add(v) { sum += v; }
	new data[] = {1, 2, 3, 4};
	Apply(data, 4, using inline Add);
	printf("hooksum=%d\n", sum);   // 10
	return 1;
}

main()
{
	Ev();
}
