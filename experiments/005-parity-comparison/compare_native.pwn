// pawn-x native parity script — compiled with the MODIFIED pawncc.
// Exercises every ability y_va / y_iterate / y_hooks offers, using the native
// language features (`___`, set* natives + `set_foreach`, `iterfunc`, `hook`).
// Prints one labeled line per ability; diff against compare_ysi.txt.

#include <open.mp>
#include <foreach>      // set* native decls + set_foreach + ITER_STOP
#include <hook>         // HOOK_CONTINUE / HOOK_STOP

// ---------- VA: variadic forwarding (`___`) ----------
NX_Fwd0(const fmt[], ...)             { printf(fmt, ___); }        // V1 forward all
NX_FwdPos(a, b, const fmt[], ...)     { a = b = 0; printf(fmt, ___); }  // V2 after fixed
NX_Nested(const fmt[], ...)           { NX_Fwd0(fmt, ___); }       // V3 nested forward
NX_ToBuf(dst[], size, const fmt[], ...) { format(dst, size, fmt, ___); } // V4 format into buffer
NX_Ret(const fmt[], ...)              { new s[96]; format(s, sizeof s, fmt, ___); return s; } // V5 format+return

// ---------- HOOK: chain on a plain (non-callback) function ----------
hook NX_Chain(a) { printf("[HOOK] chain A a=%d", a); return HOOK_CONTINUE; }
hook NX_Chain(a) { printf("[HOOK] chain B a=%d", a); return HOOK_STOP; }        // halts here
hook NX_Chain(a) { printf("[HOOK] chain C MUSTNOTRUN"); return HOOK_CONTINUE; }

// ---------- HOOK: two bodies on a REAL server callback ----------
hook OnGameModeInit() { printf("[HOOK] realcb A"); return HOOK_CONTINUE; }
hook OnGameModeInit() { printf("[HOOK] realcb B"); return HOOK_CONTINUE; }

// ---------- generator ----------
iterfunc stock NX_Range(cur, lo, hi)
{
	if (cur == ITER_STOP) return (lo < hi) ? lo : ITER_STOP;
	if (cur + 1 < hi) return cur + 1;
	return ITER_STOP;
}

new gset[16];
new grid[3][8];

main()
{
	new buf[96];

	// ---- VA ----
	NX_Fwd0("[VA] fwd0 %d %d %s", 1, 2, "x");
	NX_FwdPos(7, 8, "[VA] fwdpos %d %s", 9, "y");
	NX_Nested("[VA] nested %d", 42);
	NX_ToBuf(buf, sizeof buf, "Hello %d %04x %s", 99, 0x1F, "woop");
	printf("[VA] format %s", buf);
	new r[96];
	strcat((r[0] = 0, r), NX_Ret("Hi %d %s", 5, "z"));
	printf("[VA] return %s", r);

	// ---- SET ops ----
	setinit(gset);
	printf("[SET] add %d %d dup %d", setadd(gset, 30), setadd(gset, 10), setadd(gset, 10));
	setadd(gset, 20);
	printf("[SET] len %d has20 %d has99 %d", setlen(gset), sethas(gset, 20), sethas(gset, 99));
	printf("[SET] free %d", setfree(gset));                 // {10,20,30} -> 0
	printf("[SET] get0 %d get2 %d", setget(gset, 0), setget(gset, 2)); // 10, 30
	new al = setalloc(gset);
	printf("[SET] alloc %d then len %d", al, setlen(gset));            // 0, 4
	new rnd = setrandom(gset);
	printf("[SET] random-is-member %d", sethas(gset, rnd));
	setremove(gset, 0);                                     // undo the alloc
	printf("[SET] remove-> len %d has0 %d", setlen(gset), sethas(gset, 0));

	// ---- FOREACH ----
	print("[FE] forward");
	set_foreach (new v : gset) printf("  %d", v);
	print("[FE] reverse");
	set_foreach (new v : Reverse(gset)) printf("  %d", v);
	print("[FE] break-at>15");
	set_foreach (new v : gset) { if (v > 15) break; printf("  %d", v); }
	print("[FE] continue-skip20");
	set_foreach (new v : gset) { if (v == 20) continue; printf("  %d", v); }

	// ---- multi-dimensional / nested sets ----
	for (new k = 0; k < 3; k++) setinit(grid[k]);
	setadd(grid[1], 5); setadd(grid[1], 2); setadd(grid[2], 7);
	print("[FE] multidim grid[1]");
	set_foreach (new p : grid[1]) printf("  %d", p);

	// ---- generator ----
	print("[FE] gen-Range(0,4)");
	set_foreach (new i : NX_Range(0, 4)) printf("  %d", i);

	// ---- hook chain on a plain function ----
	NX_Chain(99);

	// ---- GAP marker: runtime replace/add/remove not supported natively ----
	printf("[HOOK] runtime-replace UNSUPPORTED (compile-time chain only)");

	// ---- safe remove of the CURRENTLY-iterated element (run last: YSI hangs here) ----
	print("[FE] saferemove-20");
	set_foreach (new v : gset) { if (v == 20) setremove(gset, 20); else printf("  keep %d", v); }
	printf("[FE] after-saferemove len %d has20 %d", setlen(gset), sethas(gset, 20));

	printf("[DONE] native");
}
