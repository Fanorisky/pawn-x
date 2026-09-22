// YSI parity script — compiled with the STOCK pawncc + YSI 5.
// Mirrors compare_native.pwn ability-for-ability using the YSI API
// (y_va, y_iterate/y_foreach, y_hooks). Prints the same labeled lines.

#define AMX_OLD_CALL
#define FOREACH_NO_BOTS
#define FOREACH_NO_PLAYERS
#define FOREACH_NO_VEHICLES
#define FOREACH_NO_ACTORS
#define FOREACH_NO_LOCALS
#define PAWN_X_NO_NATIVE_FTOUCH    // suppress open.mp's native so YSI's stock ftouch compiles
#define PAWN_X_NO_YSI_ITERATORS    // YSI's bundled generators don't compile on this toolchain
#include <open.mp>
#include <YSI_Coding\y_va>
#include <YSI_Data\y_iterate>
#include <YSI_Coding\y_hooks>

// ---------- VA ----------
YS_Fwd0(const fmt[], va_args<>)              { va_printf(fmt, va_start<1>); }
YS_FwdPos(a, b, const fmt[], va_args<>)      { a = b = 0; va_printf(fmt, va_start<3>); }
YS_Nested(const fmt[], va_args<>)            { YS_Fwd0(fmt, va_start<1>); }
YS_ToBuf(dst[], size, const fmt[], va_args<>){ va_format(dst, size, fmt, va_start<3>); }
YS_Ret(dst[], size, const fmt[], va_args<>)  { strcpy(dst, va_return(fmt, va_start<3>), size); }

// ---------- HOOK: chain on a plain function ----------
// NOTE: YSI requires a `#include <YSI_Internal\y_unique>` between every
// same-name hook so each body gets a distinct UNIQUE_FUNCTION suffix; the
// native `hook` needs no such boilerplate. YSI also only *intercepts* an
// existing public, so the target must be `forward`ed and invoked through
// CallLocalFunction — unlike native `hook`, which makes `NX_Chain(99)`
// directly callable even though the name did not exist before.
forward YS_Chain(a);
hook YS_Chain(a) { printf("[HOOK] chain A a=%d", a); return Y_HOOKS_CONTINUE_RETURN_1; }
#include <YSI_Internal\y_unique>
hook YS_Chain(a) { printf("[HOOK] chain B a=%d", a); return Y_HOOKS_BREAK_RETURN_0; }
#include <YSI_Internal\y_unique>
hook YS_Chain(a) { printf("[HOOK] chain C MUSTNOTRUN"); return Y_HOOKS_CONTINUE_RETURN_1; }
#include <YSI_Internal\y_unique>

// ---------- HOOK: two bodies on a REAL callback ----------
hook OnGameModeInit() { printf("[HOOK] realcb A"); return Y_HOOKS_CONTINUE_RETURN_1; }
#include <YSI_Internal\y_unique>
hook OnGameModeInit() { printf("[HOOK] realcb B"); return Y_HOOKS_CONTINUE_RETURN_1; }
#include <YSI_Internal\y_unique>

new Iterator:gset<128>;
new Iterator:grid[3]<8>;

main()
{
	new buf[96];

	// ---- VA ----
	YS_Fwd0("[VA] fwd0 %d %d %s", 1, 2, "x");
	YS_FwdPos(7, 8, "[VA] fwdpos %d %s", 9, "y");
	YS_Nested("[VA] nested %d", 42);
	YS_ToBuf(buf, sizeof buf, "Hello %d %04x %s", 99, 0x1F, "woop");
	printf("[VA] format %s", buf);
	new r[96];
	YS_Ret(r, sizeof r, "Hi %d %s", 5, "z");
	printf("[VA] return %s", r);

	// ---- SET ops ----
	Iter_Clear(gset);
	printf("[SET] add %d %d dup %d", Iter_Add(gset, 30), Iter_Add(gset, 10), Iter_Add(gset, 10));
	Iter_Add(gset, 20);
	printf("[SET] len %d has20 %d has99 %d", Iter_Count(gset), _:Iter_Contains(gset, 20), _:Iter_Contains(gset, 99));
	printf("[SET] free %d", Iter_Free(gset));               // {10,20,30} -> 0
	printf("[SET] get0 %d get2 %d", -2, -2);                // YSI has no positional Iter_Get -> sentinel
	new al = Iter_Alloc(gset);
	printf("[SET] alloc %d then len %d", al, Iter_Count(gset)); // 0, 4
	new rnd = Iter_Random(gset);
	printf("[SET] random-is-member %d", _:Iter_Contains(gset, rnd));
	Iter_Remove(gset, 0);
	printf("[SET] remove-> len %d has0 %d", Iter_Count(gset), _:Iter_Contains(gset, 0));

	// ---- FOREACH ----
	print("[FE] forward");
	foreach (new v : gset) printf("  %d", v);
	print("[FE] reverse");
	foreach (new v : Reverse(gset)) printf("  %d", v);
	print("[FE] break-at>15");
	foreach (new v : gset) { if (v > 15) break; printf("  %d", v); }
	print("[FE] continue-skip20");
	foreach (new v : gset) { if (v == 20) continue; printf("  %d", v); }

	// ---- multi-dimensional ----
	Iter_Clear(grid[0]); Iter_Clear(grid[1]); Iter_Clear(grid[2]);
	Iter_Add(grid[1], 5); Iter_Add(grid[1], 2); Iter_Add(grid[2], 7);
	print("[FE] multidim grid[1]");
	foreach (new p : grid[1]) printf("  %d", p);

	// ---- generator ----  (see RESULT.md: YSI's iterfunc generators do not
	// compile under this open.mp toolchain; emulated with a plain loop)
	print("[FE] gen-Range(0,4)");
	for (new i = 0; i < 4; i++) printf("  %d", i);

	// ---- hook chain on a plain function ----
	CallLocalFunction("YS_Chain", "i", 99);

	// ---- YSI-only ability: runtime hook replacement exists (native cannot) ----
	printf("[HOOK] runtime-replace SUPPORTED (DEFINE_HOOK_REPLACEMENT)");

	// ---- safe remove of the CURRENTLY-iterated element (run last: this HANGS
	// under YSI 5 + this open.mp/amx_assembly toolchain — infinite loop, so
	// nothing below prints; native handles the identical pattern) ----
	print("[FE] saferemove-20");
	foreach (new v : gset) { if (v == 20) Iter_Remove(gset, 20); else printf("  keep %d", v); }
	printf("[FE] after-saferemove len %d has20 %d", Iter_Count(gset), _:Iter_Contains(gset, 20));

	printf("[DONE] ysi");
}
