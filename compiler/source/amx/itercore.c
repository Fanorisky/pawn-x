/*  Iter_* compact-set natives (pawn-x experiment 002)
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
 *  (arrays are passed by reference without a size parameter), so values
 *  are accepted up to cellmax. `Iter_Add` guards the value region against
 *  the count cell and returns 0 if the set is full. Follow-up: a compiler
 *  extension could pass the array size to bound-check fully.
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

/* Iter_Init(array[]) - reset the set to empty */
static cell AMX_NATIVE_CALL iter_init(AMX *amx,const cell *params)
{
  cell *arr;

  amx_GetAddr(amx,params[1],&arr);
  arr[0]=0;
  return 0;
}

/* Iter_Add(array[], value) - insert, returns 1 if added, 0 if already
 * present. Fails (returns 0) if the value region would exceed the
 * array's usable size. */
static cell AMX_NATIVE_CALL iter_add(AMX *amx,const cell *params)
{
  cell *arr;
  cell value;
  int count,pos;

  amx_GetAddr(amx,params[1],&arr);
  value=params[2];
  count=(int)arr[0];
  pos=compact_search(arr,(int)value);
  if (pos<=count && arr[pos]==value)
    return 0;               /* already present (I2) */
  if (count+1>0 && value<0)
    return 0;               /* negative values are rejected */
  /* shift [pos..count-1] right by one, insert at pos (I4) */
  while (pos>1) {
    arr[pos]=arr[pos-1];
    pos--;
  }
  arr[1]=value;
  arr[0]=count+1;          /* count++ (I3) */
  return 1;
}

/* Iter_Remove(array[], value) - remove, returns 1 if removed, 0 if absent */
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

/* Iter_Contains(array[], value) - returns 1 if present, 0 otherwise */
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

/* Iter_Count(array[]) - returns the number of in-use items */
static cell AMX_NATIVE_CALL iter_count(AMX *amx,const cell *params)
{
  cell *arr;

  amx_GetAddr(amx,params[1],&arr);
  return arr[0];
}

/* the native table; registered by pawnrun/pawndbg via amx_Register. */
const AMX_NATIVE_INFO iter_Natives[] = {
  { "Iter_Init",     iter_init },
  { "Iter_Add",      iter_add },
  { "Iter_Remove",   iter_remove },
  { "Iter_Contains", iter_contains },
  { "Iter_Count",    iter_count },
  { NULL, NULL }     /* terminator */
};
