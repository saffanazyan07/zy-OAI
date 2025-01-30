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

#ifndef __TIME_CPU_DEFS__H__
#define __TIME_CPU_DEFS__H__
#include <stdint.h>

#if defined(__x86_64__) || defined(__i386__) || defined(__arm__) || defined(__aarch64__)
  typedef long long oai_cputime_t;
#else
  #error "building on unsupported CPU architecture"
#endif

#if defined(__i386__)
static inline unsigned long long rdtsc_oai(void) __attribute__((always_inline));
static inline unsigned long long rdtsc_oai(void) {
  unsigned long long int x;
  __asm__ volatile (".byte 0x0f, 0x31" : "=A" (x));
  return x;
}
#elif defined(__x86_64__)
static inline unsigned long long rdtsc_oai(void) __attribute__((always_inline));
static inline unsigned long long rdtsc_oai(void) {
  unsigned long long a, d;
  __asm__ volatile ("rdtsc" : "=a" (a), "=d" (d));
  return (d<<32) | a;
}
#elif defined(__aarch64__)
static inline uint64_t rdtsc_oai(void) __attribute__((always_inline));
static inline uint64_t rdtsc_oai(void)
{
	  uint64_t r = 0;
	    asm volatile("mrs %0, cntvct_el0" : "=r"(r));
	      return r;
}

#elif defined(__arm__) 
static inline uint32_t rdtsc_oai(void) __attribute__((always_inline));
static inline uint32_t rdtsc_oai(void) {
  uint32_t r = 0;
  asm volatile("mrc p15, 0, %0, c9, c13, 0" : "=r"(r) );
  return r;
}
#endif

#endif
