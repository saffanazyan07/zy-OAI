/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.1  (the "License"); you may not use this file
 * except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.openairinterface.org/?page_id=698
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------------------
 * For more information about the OpenAirInterface (OAI) Software Alliance:
 *      contact@openairinterface.org
 */

#ifndef __COMMON_UTILS_ALLOCATOR__H__
#define __COMMON_UTILS_ALLOCATOR__H__

#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include "common/utils/time_cpu.h"
//#define TRACE_MEMSET

#define malloc16(x) memalign(32,x)
#define free16(y,x) free(y)
#define bigmalloc malloc
#define bigmalloc16 malloc16
#define openair_free(y,x) free((y))

#define free_and_zero(PtR) do {     \
    if (PtR) {           \
      free(PtR);         \
      PtR = NULL;        \
    }                    \
  } while (0)

static inline void *malloc16_clear( size_t size ) {
  void *ptr = memalign(32, size+32);
  if(!ptr) abort();
  memset( ptr, 0, size );
  return ptr;
}

static inline void *calloc_or_fail(size_t nmemb, size_t size)
{
  void *ptr = calloc(nmemb, size);

  if (ptr == NULL) {
    fprintf(stderr, "Failed to calloc() %zu elements of %zu bytes: out of memory", nmemb, size);
    abort();
  }
  return ptr;
}

static inline void *malloc_or_fail(size_t size)
{
  void *ptr = malloc(size);
  if (ptr == NULL) {
    fprintf(stderr, "Failed to malloc() %zu bytes: out of memory", size);
    exit(EXIT_FAILURE);
  }
  return ptr;
}

#ifdef TRACE_MEMSET
/*
  example trace, you can parse the output like with:
  awk '/^memset/ {nb[$2]++; sz[$2]+=$3;tot_time[$2]+=$5} END {for (i in nb) print tot_time[i], sz[i], nb[i], i}' memset.ue|sort -n
  this will print, sorted the total time spent in each memset of the entire code
*/
static inline void * oai_memset(const char *f, const int l, void *s, int c, size_t n) {
  oai_cputime_t st=rdtsc_oai();
  void * ret=memset(s,c,n);
  printf("memset %s:%d %lu time %llu\n",f,l, n,rdtsc_oai()-st );
return ret;
}
#define memset(a...) oai_memset(__FILE__,__LINE__,a)
#undef bzero
#define bzero(Ptr,Size) oai_memset(__FILE__,__LINE__,Ptr,0,Size)
#endif

#endif
