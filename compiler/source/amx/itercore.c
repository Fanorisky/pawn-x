/*  Compact-set natives (setinit/setadd/setremove/sethas/setlen) (pawn-x experiment 002)
 *
 *  Copyright (c) ITB CompuPhase, 1997-2016
 *  Modified for pawn-x (https://github.com/Fanorisky/pawn-x)
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may not
 *  use this file except in compliance with the License. You may obtain a copy
 *  of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 *  License for the specific language governing permissions and limitations
 *  under the License.
 *
 *  A native compact sorted set over a script global array.
 *
 *  Layout, for `new X[cap];`:
 *    X[0]            = count of in-use items
 *    X[1 .. count]   = the distinct in-use values, strictly ascending
 *    X[count+1 ..]   = free
 *
 *  Invariants (reference: experiments/002-foreach/probe/compact_set.pwn):
 *    I1 sorted     X[1..count] strictly ascending
 *    I2 nodup      a value is present at most once
 *    I3 count      X[0] == number of in-use items
 *    I4 addshift   add inserts in sorted position, shifting the tail
 *    I5 rmtail     remove shifts the tail left, count--
 *
 *  Range limitation (V1): the native cannot know the array's capacity
 *  (arrays are passed by reference without a size parameter), so it does
 *  NOT bound-check inserts. The caller must size the array for the number
 *  of values it will hold; a `new X[cap]` set holds up to cap-1 values
 *  (slot 0 is the count). Follow-up: a compiler extension could pass the
 *  array size so `setadd` can reject a full set.
 */

#include "amx.h"

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

  amx_GetAddr(amx,params[1],&arr);
  arr[0]=0;
  return 0;
}

/* setadd(array[], value) - insert, returns 1 if added, 0 if the value
 * was already present or negative. V1 does NOT bound-check against the
 * array capacity (see the range limitation in the file header): the
 * caller must size the array for the number of values it will hold. */
static cell AMX_NATIVE_CALL iter_add(AMX *amx,const cell *params)
{
  cell *arr;
  cell value;
  int count,pos,i;

  amx_GetAddr(amx,params[1],&arr);
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

  amx_GetAddr(amx,params[1],&arr);
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

  amx_GetAddr(amx,params[1],&arr);
  value=params[2];
  count=(int)arr[0];
  pos=compact_search(arr,(int)value);
  return (pos<=count && arr[pos]==value) ? 1 : 0;
}

/* setlen(array[]) - returns the number of in-use items */
static cell AMX_NATIVE_CALL iter_count(AMX *amx,const cell *params)
{
  cell *arr;

  amx_GetAddr(amx,params[1],&arr);
  return arr[0];
}

/* setfree(array[]) - returns the smallest non-negative integer NOT in the set
 * (an id-allocation helper). Values are sorted ascending in arr[1..count], so
 * the first index k where arr[k] differs from its expected value k-1 is the
 * gap; if the run is dense (0..count-1) the answer is count. */
static cell AMX_NATIVE_CALL iter_free(AMX *amx,const cell *params)
{
  cell *arr;
  int count,k;
  cell expected;

  amx_GetAddr(amx,params[1],&arr);
  count=(int)arr[0];
  expected=0;
  for (k=1; k<=count; k++) {
    if (arr[k]!=expected)
      break;                  /* found the first gap */
    expected++;
  }
  return expected;
}

/* setrandom(array[]) - returns a uniformly chosen in-use value, or -1 if the
 * set is empty. Uses a small self-contained xorshift PRNG so it does not
 * depend on the host's rand() state. */
static cell AMX_NATIVE_CALL iter_random(AMX *amx,const cell *params)
{
  static unsigned long seed=2463534242UL;   /* xorshift32 seed */
  cell *arr;
  int count;

  amx_GetAddr(amx,params[1],&arr);
  count=(int)arr[0];
  if (count<=0)
    return -1;                /* empty set */
  seed^=seed<<13;
  seed^=seed>>17;
  seed^=seed<<5;
  return arr[1+(int)(seed%(unsigned long)count)];
}

/* the native table; registered by pawnruns (the test runner) via amx_Register. */
const AMX_NATIVE_INFO iter_Natives[] = {
  { "setinit",   iter_init },
  { "setadd",    iter_add },
  { "setremove", iter_remove },
  { "sethas",    iter_contains },
  { "setlen",    iter_count },
  { "setfree",   iter_free },
  { "setrandom", iter_random },
  { NULL, NULL }     /* terminator */
};
