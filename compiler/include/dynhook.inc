#if defined _INC_dynhook
	#endinput
#endif
#define _INC_dynhook

/**
 * <library name="dynhook">
 *  pawn-x companion-plugin (exp 006, phase A): a RUNTIME hook registry. The
 *  compiler's `hook` keyword fixes a chain at compile time; these natives add
 *  the runtime-only capability — add / remove / replace handlers while the
 *  server runs — with no bytecode scanning (chains live in the plugin, dispatch
 *  is plain amx_FindPublic + amx_Exec). Requires the `dynhook` legacy plugin.
 *
 *    dynhook_add("OnFoo", "MyHandler");     // register a public by name
 *    dynhook_call("OnFoo", "i", 7);         // run the chain, forwarding args
 *    dynhook_replace("OnFoo", "A", "B");    // swap a handler at runtime
 *
 *  `dynhook_call` fmt: 'i'/'d'/'f' forward one cell, 's' forwards a string.
 * </library>
 */

native dynhook_add(const event[], const pub[]);
native dynhook_remove(const event[], const pub[]);
native dynhook_replace(const event[], const oldpub[], const newpub[]);
native dynhook_clear(const event[]);
native dynhook_count(const event[]);
native dynhook_call(const event[], const fmt[], ...);
