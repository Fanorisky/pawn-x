/*  iterset — open.mp legacy plugin registering the pawn-x compact-set natives
 *  (setinit/setadd/setremove/sethas/setlen) on the real omp-server.
 *
 *  The compact-set logic is copied verbatim from
 *  compiler/source/amx/itercore.c, with every amx_GetAddr() call routed
 *  through g_GetAddr (the pointer handed to us by Load()), so the plugin
 *  needs only amx.h's type declarations — it does not link amx.c.
 *
 *  Layout, for `new X[cap];`:
 *    X[0]          = count of in-use items
 *    X[1 .. count] = the distinct in-use values, strictly ascending
 */

#include "amx.h"

/* AMX exports table: ppData[PLUGIN_DATA_AMX_EXPORTS] is a void** of AMX
 * function pointers (indices verified from open.mp Plugin.cpp AMX_FUNCTIONS[]). */
#define PLUGIN_DATA_AMX_EXPORTS 16
#define AMX_EXPORT_GetAddr      13
#define AMX_EXPORT_Register     33

/* SUPPORTS flags */
#define SUPPORTS_VERSION      0x0200
#define SUPPORTS_AMX_NATIVES  0x00010000

typedef int (AMXAPI *GetAddr_t)(AMX *amx, cell amx_addr, cell **phys_addr);
typedef int (AMXAPI *Register_t)(AMX *amx, const AMX_NATIVE_INFO *nativelist, int number);

static GetAddr_t  g_GetAddr  = 0;
static Register_t g_Register = 0;

/* ---- compact-set logic (verbatim from itercore.c; amx_GetAddr -> g_GetAddr) ---- */

/* binary search over the compact run X[1..count].
 * returns the slot index of an equal value, or the insertion point
 * (first slot holding a value greater than `value`) if absent */
static int compact_search(cell *arr,int value)
{
  int count=(int)arr[0];
  int lo=1,hi=count;
  while (lo<=hi) {
    int mid=(lo+hi)/2;
    if (arr[mid]<value)
      lo=mid+1;
    else if (arr[mid]>value)
      hi=mid-1;
    else
      return mid;
  }
  return lo;
}

/* setinit(array[]) - reset the set to empty */
static cell AMX_NATIVE_CALL iter_init(AMX *amx,const cell *params)
{
  cell *arr;

  g_GetAddr(amx,params[1],&arr);
  arr[0]=0;
  return 0;
}

/* setadd(array[], value) - insert, returns 1 if added, 0 if the value
 * was already present or negative. V1 does NOT bound-check against the
 * array capacity: the caller must size the array for the values it holds. */
static cell AMX_NATIVE_CALL iter_add(AMX *amx,const cell *params)
{
  cell *arr;
  cell value;
  int count,pos,i;

  g_GetAddr(amx,params[1],&arr);
  value=params[2];
  if (value<0)
    return 0;               /* negative values are rejected */
  count=(int)arr[0];
  pos=compact_search(arr,(int)value);
  if (pos<=count && arr[pos]==value)
    return 0;               /* already present (I2) */
  /* shift the tail [pos..count] up by one slot, then insert at pos (I4) */
  i=count;
  while (i>=pos) {
    arr[i+1]=arr[i];
    i--;
  }
  arr[pos]=value;
  arr[0]=count+1;          /* count++ (I3) */
  return 1;
}

/* setremove(array[], value) - remove, returns 1 if removed, 0 if absent */
static cell AMX_NATIVE_CALL iter_remove(AMX *amx,const cell *params)
{
  cell *arr;
  cell value;
  int count,pos;

  g_GetAddr(amx,params[1],&arr);
  value=params[2];
  count=(int)arr[0];
  pos=compact_search(arr,(int)value);
  if (pos>count || arr[pos]!=value)
    return 0;               /* not present */
  /* shift [pos+1..count-1] left by one (I5) */
  while (pos<count) {
    arr[pos]=arr[pos+1];
    pos++;
  }
  arr[0]=count-1;          /* count-- (I3) */
  return 1;
}

/* sethas(array[], value) - returns 1 if present, 0 otherwise */
static cell AMX_NATIVE_CALL iter_contains(AMX *amx,const cell *params)
{
  cell *arr;
  cell value;
  int count,pos;

  g_GetAddr(amx,params[1],&arr);
  value=params[2];
  count=(int)arr[0];
  pos=compact_search(arr,(int)value);
  return (pos<=count && arr[pos]==value) ? 1 : 0;
}

/* setlen(array[]) - returns the number of in-use items */
static cell AMX_NATIVE_CALL iter_count(AMX *amx,const cell *params)
{
  cell *arr;

  g_GetAddr(amx,params[1],&arr);
  return arr[0];
}

static const AMX_NATIVE_INFO iter_Natives[] = {
  { "setinit",   iter_init },
  { "setadd",    iter_add },
  { "setremove", iter_remove },
  { "sethas",    iter_contains },
  { "setlen",    iter_count },
  { NULL, NULL }
};

/* ---- open.mp legacy-plugin ABI ---- */

unsigned int Supports(void)
{
  return SUPPORTS_VERSION | SUPPORTS_AMX_NATIVES;   /* 0x00010200 */
}

int Load(const void * const * ppData)
{
  void **amxExports = (void **)ppData[PLUGIN_DATA_AMX_EXPORTS];
  g_GetAddr  = (GetAddr_t) amxExports[AMX_EXPORT_GetAddr];
  g_Register = (Register_t)amxExports[AMX_EXPORT_Register];
  return 1;   /* nonzero = success */
}

int AmxLoad(AMX *amx)
{
  g_Register(amx, iter_Natives, -1);
  return 0;   /* AMX_ERR_NONE */
}

int AmxUnload(AMX *amx)
{
  (void)amx;
  return 0;
}

int Unload(void)
{
  return 0;
}
