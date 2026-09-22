/*  dynhook — pawn-x companion plugin, PHASE A (experiment 006)
 *
 *  The runtime pillar of pawn-x's two-pillar hook design. The compiler's `hook`
 *  keyword builds a hook chain fixed at COMPILE time (exp 004); this plugin adds
 *  the one thing a compiler cannot: add / remove / replace handlers at RUNTIME,
 *  the capability YSI's DEFINE_HOOK_REPLACEMENT provides — but here with no
 *  bytecode scanning or rewriting. Chains live in C++; dispatch is plain
 *  amx_FindPublic + amx_Exec. Phase A is an explicit named-event registry
 *  (`dynhook_call`); Phase B will make interception of built-in callbacks
 *  transparent via the open.mp component SDK.
 *
 *  Native API (see dynhook.inc):
 *    dynhook_add(const event[], const pub[])            append handler -> new count
 *    dynhook_remove(const event[], const pub[])         remove first match -> 1/0
 *    dynhook_replace(const event[], const old[], const new[])  in-place -> 1/0
 *    dynhook_clear(const event[])                       drop the whole chain
 *    dynhook_count(const event[])                       handlers registered
 *    dynhook_call(const event[], const fmt[], ...)      run the chain in order,
 *                                                       fmt: i/d/f = cell, s = string;
 *                                                       returns handlers invoked
 *
 *  Legacy-plugin ABI verified against open.mp Pawn/main.cpp AMX_FUNCTIONS[]
 *  (index order is the canonical SA-MP one). Built -m32 -shared.
 */

#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <set>

extern "C" {
#include "amx.h"
}
#include "subhook.h"

/* ---- open.mp AMX exports table (ppData[16]); indices from AMX_FUNCTIONS[] ---- */
#define PLUGIN_DATA_AMX_EXPORTS 16
#define IDX_Allot       3
#define IDX_Exec        7
#define IDX_FindPublic  9
#define IDX_GetPublic   15
#define IDX_GetAddr     13
#define IDX_NumPublics  26
#define IDX_Push        29
#define IDX_PushString  31
#define IDX_Register    33
#define IDX_Release     34

#define SUPPORTS_VERSION      0x0200
#define SUPPORTS_AMX_NATIVES  0x00010000

typedef int (AMXAPI *Exec_t)(AMX*, cell*, int);
typedef int (AMXAPI *FindPublic_t)(AMX*, const char*, int*);
typedef int (AMXAPI *GetPublic_t)(AMX*, int, char*);
typedef int (AMXAPI *NumPublics_t)(AMX*, int*);
typedef int (AMXAPI *GetAddr_t)(AMX*, cell, cell**);
typedef int (AMXAPI *Push_t)(AMX*, cell);
typedef int (AMXAPI *PushString_t)(AMX*, cell*, cell**, const char*, int, int);
typedef int (AMXAPI *Register_t)(AMX*, const AMX_NATIVE_INFO*, int);
typedef int (AMXAPI *Release_t)(AMX*, cell);

static Exec_t       g_ExecOrig;    /* raw exports pointer (to build the hook) */
static Exec_t       g_Exec;        /* trampoline: call the REAL amx_Exec, bypassing our hook */
static FindPublic_t g_FindPublic;
static GetPublic_t  g_GetPublic;
static NumPublics_t g_NumPublics;
static GetAddr_t    g_GetAddr;
static Push_t       g_Push;
static PushString_t g_PushString;
static Register_t   g_Register;
static Release_t    g_Release;

/* event name -> ordered list of public handler names */
static std::map<std::string, std::vector<std::string> > g_chains;

/* ---- Phase B (transparent interception) state ---- */
static subhook_t g_execHook = 0;
/* callback names whose chain fires automatically when the host runs them */
static std::set<std::string> g_intercept;
/* per-AMX public index -> name (built at AmxLoad) so the Exec hook can map
 * an incoming call index back to a callback name */
static std::map<AMX*, std::vector<std::string> > g_pubNames;
/* re-entrancy guard: while dispatching handlers we must not re-intercept */
static int g_inDispatch = 0;

/* read a NUL-terminated pawn string at an AMX address into std::string */
static std::string amx_read_str(AMX *amx, cell amx_addr)
{
  cell *phys = 0;
  std::string s;
  if (g_GetAddr(amx, amx_addr, &phys) != 0 || phys == 0)
    return s;
  for (int i = 0; phys[i] != 0; i++)
    s += (char)(phys[i] & 0xFF);
  return s;
}

/* dynhook_add(const event[], const pub[]) -> new handler count */
static cell AMX_NATIVE_CALL n_add(AMX *amx, const cell *params)
{
  std::string ev  = amx_read_str(amx, params[1]);
  std::string pub = amx_read_str(amx, params[2]);
  g_chains[ev].push_back(pub);
  return (cell)g_chains[ev].size();
}

/* dynhook_remove(const event[], const pub[]) -> 1 if a handler was removed */
static cell AMX_NATIVE_CALL n_remove(AMX *amx, const cell *params)
{
  std::string ev  = amx_read_str(amx, params[1]);
  std::string pub = amx_read_str(amx, params[2]);
  std::vector<std::string> &v = g_chains[ev];
  for (size_t i = 0; i < v.size(); i++) {
    if (v[i] == pub) { v.erase(v.begin() + i); return 1; }
  }
  return 0;
}

/* dynhook_replace(const event[], const old[], const new[]) -> 1 if replaced */
static cell AMX_NATIVE_CALL n_replace(AMX *amx, const cell *params)
{
  std::string ev   = amx_read_str(amx, params[1]);
  std::string oldp = amx_read_str(amx, params[2]);
  std::string newp = amx_read_str(amx, params[3]);
  std::vector<std::string> &v = g_chains[ev];
  for (size_t i = 0; i < v.size(); i++) {
    if (v[i] == oldp) { v[i] = newp; return 1; }
  }
  return 0;
}

/* dynhook_clear(const event[]) -> handlers dropped */
static cell AMX_NATIVE_CALL n_clear(AMX *amx, const cell *params)
{
  std::string ev = amx_read_str(amx, params[1]);
  std::map<std::string, std::vector<std::string> >::iterator it = g_chains.find(ev);
  if (it == g_chains.end()) return 0;
  cell n = (cell)it->second.size();
  g_chains.erase(it);
  return n;
}

/* dynhook_count(const event[]) -> handlers registered */
static cell AMX_NATIVE_CALL n_count(AMX *amx, const cell *params)
{
  std::string ev = amx_read_str(amx, params[1]);
  std::map<std::string, std::vector<std::string> >::iterator it = g_chains.find(ev);
  return (it == g_chains.end()) ? 0 : (cell)it->second.size();
}

/* dynhook_call(const event[], const fmt[], ...) -> number of handlers invoked.
 * fmt: 'i'/'d'/'f' forward one cell, 's' forwards a string. Variadic args are
 * passed by reference, so params[3+n] is the AMX address of arg n. Each handler
 * is called in registration order; a snapshot is taken so a handler may safely
 * add/remove/replace within the same event. */
static cell AMX_NATIVE_CALL n_call(AMX *amx, const cell *params)
{
  std::string ev  = amx_read_str(amx, params[1]);
  std::string fmt = amx_read_str(amx, params[2]);

  std::map<std::string, std::vector<std::string> >::iterator it = g_chains.find(ev);
  if (it == g_chains.end() || it->second.empty())
    return 0;
  std::vector<std::string> chain = it->second;   /* snapshot */

  int nargs = (int)fmt.size();
  cell invoked = 0;

  for (size_t h = 0; h < chain.size(); h++) {
    int idx = -1;
    if (g_FindPublic(amx, chain[h].c_str(), &idx) != 0)
      continue;                                  /* no such public in this .amx */

    cell first_str = -1;                         /* lowest heap addr allotted */
    /* push right-to-left (AMX calling convention) */
    for (int n = nargs - 1; n >= 0; n--) {
      cell arg_addr = params[3 + n];
      char sp = fmt[n];
      if (sp == 's') {
        std::string s = amx_read_str(amx, arg_addr);
        cell amx_addr = -1;
        g_PushString(amx, &amx_addr, 0, s.c_str(), 0, 0);
        if (first_str == -1) first_str = amx_addr;
      } else {                                   /* i/d/f and anything else: one cell */
        cell *p = 0;
        cell val = 0;
        if (g_GetAddr(amx, arg_addr, &p) == 0 && p) val = *p;
        g_Push(amx, val);
      }
    }
    cell ret = 0;
    g_Exec(amx, &ret, idx);
    if (first_str != -1)
      g_Release(amx, first_str);                 /* free any pushed strings */
    invoked++;
  }
  return invoked;
}

/* dynhook_intercept(const callback[]) -> mark a callback name for transparent
 * runtime interception: once marked, the chain registered under that name fires
 * automatically whenever the host runs that public (no explicit dynhook_call). */
static cell AMX_NATIVE_CALL n_intercept(AMX *amx, const cell *params)
{
  std::string cb = amx_read_str(amx, params[1]);
  g_intercept.insert(cb);
  return 1;
}

/* The amx_Exec inline hook. Every public the host runs passes through here.
 * If the called public is a marked interception target, we run the original and
 * then dispatch the runtime chain with the same arguments (post-hook). All of
 * our own dispatch goes through the trampoline (g_Exec), never re-entering this
 * hook; a guard covers any nested host call a handler might trigger. */
static int AMXAPI Exec_hook(AMX *amx, cell *retval, int index)
{
  if (g_inDispatch || index < 0)          /* AMX_EXEC_MAIN/CONT + our own dispatch */
    return g_Exec(amx, retval, index);

  std::map<AMX*, std::vector<std::string> >::iterator mi = g_pubNames.find(amx);
  if (mi == g_pubNames.end() || index >= (int)mi->second.size())
    return g_Exec(amx, retval, index);
  const std::string name = mi->second[index];
  if (name.empty() || g_intercept.find(name) == g_intercept.end())
    return g_Exec(amx, retval, index);

  /* capture the arguments BEFORE the original runs (paramcount cells at STK) */
  int n = amx->paramcount;
  std::vector<cell> saved(n > 0 ? n : 0);
  AMX_HEADER *hdr = (AMX_HEADER *)amx->base;
  unsigned char *data = amx->data ? amx->data : amx->base + (uintptr_t)hdr->dat;
  for (int i = 0; i < n; i++)
    saved[i] = *(cell *)(data + (uintptr_t)amx->stk + (uintptr_t)i * sizeof(cell));

  /* run the original callback body unchanged */
  int err = g_Exec(amx, retval, index);

  /* then fire the runtime chain with the same args (snapshot so a handler may
   * add/remove/replace safely); each handler is a fresh, clean push+exec */
  std::map<std::string, std::vector<std::string> >::iterator ci = g_chains.find(name);
  if (ci != g_chains.end() && !ci->second.empty()) {
    std::vector<std::string> chain = ci->second;
    g_inDispatch++;
    for (size_t h = 0; h < chain.size(); h++) {
      int hi = -1;
      if (g_FindPublic(amx, chain[h].c_str(), &hi) != 0)
        continue;
      for (int i = n - 1; i >= 0; i--)     /* push reverse -> declared order */
        g_Push(amx, saved[i]);
      cell r = 0;
      g_Exec(amx, &r, hi);
    }
    g_inDispatch--;
  }
  return err;
}

static const AMX_NATIVE_INFO dynhook_Natives[] = {
  { "dynhook_add",     n_add },
  { "dynhook_remove",  n_remove },
  { "dynhook_replace", n_replace },
  { "dynhook_clear",   n_clear },
  { "dynhook_count",   n_count },
  { "dynhook_call",    n_call },
  { "dynhook_intercept", n_intercept },
  { 0, 0 }
};

/* ---- open.mp legacy-plugin ABI ---- */
extern "C" {

unsigned int Supports(void)
{
  return SUPPORTS_VERSION | SUPPORTS_AMX_NATIVES;
}

int Load(const void * const * ppData)
{
  void **e = (void **)ppData[PLUGIN_DATA_AMX_EXPORTS];
  g_ExecOrig   = (Exec_t)      e[IDX_Exec];
  g_FindPublic = (FindPublic_t)e[IDX_FindPublic];
  g_GetPublic  = (GetPublic_t) e[IDX_GetPublic];
  g_NumPublics = (NumPublics_t)e[IDX_NumPublics];
  g_GetAddr    = (GetAddr_t)   e[IDX_GetAddr];
  g_Push       = (Push_t)      e[IDX_Push];
  g_PushString = (PushString_t)e[IDX_PushString];
  g_Register   = (Register_t)  e[IDX_Register];
  g_Release    = (Release_t)   e[IDX_Release];

  /* Phase B: inline-hook the real amx_Exec so host callback dispatch passes
   * through Exec_hook. Portable across SA-MP and open.mp (same technique as
   * crashdetect/sampgdk). Fall back to the raw pointer if the hook fails. */
  g_execHook = subhook_new((void *)g_ExecOrig, (void *)Exec_hook, SUBHOOK_TRAMPOLINE);
  if (g_execHook && subhook_install(g_execHook) == 0)
    g_Exec = (Exec_t)subhook_get_trampoline(g_execHook);
  if (!g_Exec)
    g_Exec = g_ExecOrig;   /* interception disabled, Phase A still works */
  return 1;
}

int AmxLoad(AMX *amx)
{
  g_Register(amx, dynhook_Natives, -1);
  /* build this script's public index -> name table for the Exec hook */
  int count = 0;
  if (g_NumPublics(amx, &count) == 0 && count > 0) {
    std::vector<std::string> names((size_t)count);
    for (int i = 0; i < count; i++) {
      char nm[64];
      nm[0] = 0;
      if (g_GetPublic(amx, i, nm) == 0)
        names[(size_t)i] = nm;
    }
    g_pubNames[amx] = names;
  }
  return 0;
}

int AmxUnload(AMX *amx) { g_pubNames.erase(amx); return 0; }

int Unload(void)
{
  if (g_execHook) {
    subhook_remove(g_execHook);
    subhook_free(g_execHook);
    g_execHook = 0;
  }
  g_chains.clear();
  g_intercept.clear();
  g_pubNames.clear();
  return 0;
}

}
