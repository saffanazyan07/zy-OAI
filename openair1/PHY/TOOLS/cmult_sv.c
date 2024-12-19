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

#include "PHY/sse_intrin.h"
#include "tools_defs.h"
#include <simde/simde-common.h>
#include <simde/x86/sse.h>

void multadd_complex_vector_real_scalar(int16_t *x,
                                        int16_t alpha,
                                        int16_t *y,
                                        uint8_t zero_flag,
                                        uint32_t N)
{
  simd_q15_t alpha_128,*x_128=(simd_q15_t *)x,*y_128=(simd_q15_t*)y;
  int n;

  alpha_128 = set1_int16(alpha);
  const uint32_t num_simd_adds = N / 4;
  const uint32_t num_adds = N % 4;

  if (zero_flag == 1) {
    for (n = 0; n < num_simd_adds; n++) {
      y_128[n] = mulhi_int16(x_128[n],alpha_128);
    }
    for (n = 0; n < num_adds; n++) {
      const uint32_t offset = num_simd_adds * 4;
      y[offset + n] = (x[offset + n] * alpha) >> 16;
    }
  } else {
    for (n = 0; n < num_simd_adds; n++) {
      y_128[n] = adds_int16(y_128[n],mulhi_int16(x_128[n],alpha_128));
    }
    for (n = 0; n < num_adds; n++) {
      const uint32_t offset = num_simd_adds * 4;
      y[offset + n] += (x[offset + n] * alpha) >> 16;
    }
  }
}

void multadd_real_vector_complex_scalar(const int16_t *x, const int16_t *alpha, int16_t *y, uint32_t N)
{

  // do 8 multiplications at a time
  simd_q15_t *x_128 = (simd_q15_t *)x,
             *y_128 = (simd_q15_t *)y;

  //  printf("alpha = %d,%d\n",alpha[0],alpha[1]);
  const simd_q15_t alpha_r_128 = set1_int16(alpha[0]);
  const simd_q15_t alpha_i_128 = set1_int16(alpha[1]);

  for (uint32_t i = 0; i < N >> 3; i++) {
    const simd_q15_t yr = mulhi_s1_int16(alpha_r_128, x_128[i]);
    const simd_q15_t yi = mulhi_s1_int16(alpha_i_128, x_128[i]);
    
    {
      simd_q15_t result = simde_mm_unpacklo_epi16(yr, yi);
      simd_q15_t y128 = simde_mm_adds_epi16(
        simde_mm_loadu_si128(y_128),
        result
      );
      simde_mm_storeu_si128(y_128++, y128);
    }
    {
      simd_q15_t result = simde_mm_unpackhi_epi16(yr, yi);
      simd_q15_t y128 = simde_mm_adds_epi16(
        simde_mm_loadu_si128(y_128),
        result
      );
      simde_mm_storeu_si128(y_128++, y128);
    }
  }
}

// C compiler flags (ReleaseWithDebugInfo or Release + SIMDe)
//#define COMPILER_OPTIM "-ggdb2 -DMALLOC_CHECK_=3 -fno-delete-null-pointer-checks -O2"
//#define COMPILER_OPTIM "-O3"
//#define COMPILER_SIMDE "-DSIMDE_X86_AVX512BW_NATIVE -DSIMDE_X86_AVX512F_NATIVE -DSIMDE_X86_AVX512VL_NATIVE -DSIMDE_X86_FMA_NATIVE -DSIMDE_X86_GFNI_NATIVE -DSIMDE_X86_VPCLMULQDQ_NATIVE -DSIMDE_X86_XOP_HAVE_COM_ -DSIMDE_X86_XOP_NATIVE -DSIMDE_X86_SSE3_NATIVE -mavx512bw -march=skylake-avx512 -mtune=skylake-avx512 -march=native -pipe -Wall -Wno-packed-bitfield-compat -std=gnu11 -rdynamic -fPIC -fno-strict-aliasing -funroll-loops"

//#define AVX2_FALLBACK  // Fallback
//#define AVX2_PROTOTYPE // Release and ReleaseWithDebugInfo (393 vs. 382 (+ 33 shuffle vector = 415) lines)
#define AVX2_FASTFAST

#if defined(AVX2_FALLBACK)
#pragma message("Compiling with AVX2_FALLBACK enabled")
#else
#pragma message("Compiling with AVX2_FALLBACK disabled")
#endif

#if defined(AVX2_PROTOTYPE)
#pragma message("Compiling with AVX2_PROTOTYPE enabled")
#else
#pragma message("Compiling with AVX2_PROTOTYPE disabled")
#endif

#if defined(AVX2_FASTFAST)
#pragma message("Compiling with AVX2_FASTFAST enabled")
#else
#pragma message("Compiling with AVX2_FASTFAST disabled")
#endif

#if defined(AVX2_FALLBACK)
__attribute__((optimize("Ofast"), target("sse4.2")))
#endif // defined(AVX2_FALLBACK)
__attribute__((always_inline)) static inline
void rotate_cpx_vector_sse(const c16_t *const x, const c16_t *const alpha, c16_t *y, uint32_t N, uint16_t output_shift)
{
    // Multiply elementwise two complex vectors of N elements
    // x        - input 1    in the format  |Re0  Im0 |,......,|Re(N-1) Im(N-1)|
    //            We assume x1 with a dynamic of 15 bit maximum
    //
    // alpha      - input 2    in the format  |Re0 Im0|
    //            We assume x2 with a dynamic of 15 bit maximum
    //
    // y        - output     in the format  |Re0  Im0|,......,|Re(N-1) Im(N-1)|
    //
    // N        - the size f the vectors (this function does N cpx mpy. WARNING: N>=4;
    //
    // log2_amp - increase the output amplitude by a factor 2^log2_amp (default is 0)
    //            WARNING: log2_amp>0 can cause overflow!!

#if defined(AVX2_FALLBACK)

    // Cast inputs to appropriate types
    // Give compiler hint that x and y are 16-byte (128-bit) aligned
    const c16_t *x_128 = (const c16_t *)__builtin_assume_aligned(x, 16);
          c16_t *y_128 = (      c16_t *)__builtin_assume_aligned(y, 16);

    for (uint32_t i = 0; i < N; i++)
      y_128[i] = c16mulShift(x_128[i], *alpha, output_shift);

#else // defined(AVX2_FALLBACK)

    // Cast inputs to appropriate types
    // Give compiler hint that x and y are 16-byte (128-bit) aligned
    const simde__m128i *x_128 = (const simde__m128i *)__builtin_assume_aligned(x, 16);
          simde__m128i *y_128 = (      simde__m128i *)__builtin_assume_aligned(y, 16);

    // Precompute constants
    const simde__m128i alpha_128 = simde_mm_setr_epi16(
        alpha->r, -alpha->i, alpha->i, alpha->r,
        alpha->r, -alpha->i, alpha->i, alpha->r
    );

    // Loop over SIMD-sized chunks (4 complex numbers per iteration)
/*
    if (N % (3*4))
        printf("%d is not divisible by %d.\n", N, 3*4);
    // unroll by 3
    #if defined(__clang__)
      #pragma clang loop unroll_count(3)
    #elif defined(__GNUC__)
      #pragma GCC unroll 3
    #endif
*/
    for (uint32_t i = 0; i < (N >> 2); i++) {
        // Load input vector from x_128 (use loadu/lddqu if unaligned memory is possible)
        //simde__m128i input = simde_mm_loadu_si128(&x_128[i]);
        //simde__m128i input = simde_mm_lddqu_si128(&x_128[i]);
        simde__m128i input = simde_mm_load_si128(&x_128[i]);
        //const simde__m128i input = x_128[i];

        // Shuffle and multiply for the first pair of complex numbers
        const simde__m128i shuffled_re = simde_mm_shuffle_epi32(input, SIMDE_MM_SHUFFLE(1, 1, 0, 0));
        const simde__m128i x_re = simde_mm_srai_epi32(simde_mm_madd_epi16(shuffled_re, alpha_128), output_shift);

        // Shuffle and multiply for the second pair of complex numbers
        const simde__m128i shuffled_im = simde_mm_shuffle_epi32(input, SIMDE_MM_SHUFFLE(3, 3, 2, 2));
        const simde__m128i x_im = simde_mm_srai_epi32(simde_mm_madd_epi16(shuffled_im, alpha_128), output_shift);

        // Pack results and store (use storeu if unaligned memory is possible)
        const simde__m128i result = simde_mm_packs_epi32(x_re, x_im);
        //simde_mm_storeu_si128(&y_128[i], result);
        //simde_mm_store_si128(&y_128[i], result);
        y_128[i] = result;

    }
#endif // defined(AVX2_FALLBACK)
}

#if defined(__x86_64__) || defined (__i386__)
#if defined(AVX2_FALLBACK)
__attribute__((optimize("Ofast"), target("avx2")))
#endif // defined(AVX2_FALLBACK)
__attribute__((always_inline)) static inline
void rotate_cpx_vector_avx2(const c16_t *const x, const c16_t *const alpha, c16_t *y, uint32_t N, uint16_t output_shift)
{

#if defined(AVX2_FALLBACK)
    // Cast inputs to appropriate types
    // Give compiler hint that x and y are 32-byte (256-bit) aligned
    const c16_t *x_256 = (const c16_t *)__builtin_assume_aligned(x, 32);
          c16_t *y_256 = (      c16_t *)__builtin_assume_aligned(y, 32);

    for (uint32_t i = 0; i < N; i++)
      y_256[i] = c16mulShift(x_256[i], *alpha, output_shift);

#else // defined(AVX2_FALLBACK)

    // Cast inputs to appropriate types
    // Give compiler hint that x and y are 32-byte (256-bit) aligned
    const simde__m256i *x_256 = (const simde__m256i *)__builtin_assume_aligned(x, 32);
          simde__m256i *y_256 = (      simde__m256i *)__builtin_assume_aligned(y, 32);

#if defined(AVX2_PROTOTYPE)

    // Precompute constants
    const simde__m256i alpha_256 = simde_mm256_setr_epi16(
        alpha->r, -alpha->i, alpha->i, alpha->r,
        alpha->r, -alpha->i, alpha->i, alpha->r,
        alpha->r, -alpha->i, alpha->i, alpha->r,
        alpha->r, -alpha->i, alpha->i, alpha->r
    );

    //const simde__m128i alpha_128 = simde_mm_setr_epi16(
    //  alpha->r, -alpha->i, alpha->i, alpha->r,
    //  alpha->r, -alpha->i, alpha->i, alpha->r
    //);
    //const simde__m256i alpha_256 = simde_mm256_broadcastsi128_si256(alpha_128);

    // Loop over SIMD-sized chunks (8 complex numbers per iteration)
/*    
    if (N % (12*8))
        printf("%d is not divisible by %d.\n", N, 12*8);
    // unroll by 12
    #if defined(__clang__)
      #pragma clang loop unroll_count(12)
    #elif defined(__GNUC__)
      #pragma GCC unroll 12
    #endif
*/
    for (uint32_t i = 0; i < (N >> 3); i++) {
        // Load input vector from x_256 (use loadu/lddqu if unaligned memory is possible)
        //simde__m256i input = simde_mm256_loadu_si256(&x_256[i]);
        //simde__m256i input = simde_mm256_lddqu_si256(&x_256[i]);
        //simde__m256i input = simde_mm256_load_si256(&x_256[i]);
        const simde__m256i input = x_256[i];

        // Shuffle and multiply for the first 4 complex numbers
        const simde__m256i shuffled_re = simde_mm256_shuffle_epi32(input, SIMDE_MM_SHUFFLE(1, 1, 0, 0));
        const simde__m256i x_re = simde_mm256_srai_epi32(simde_mm256_madd_epi16(shuffled_re, alpha_256), output_shift);

        // Shuffle and multiply for the second 4 complex numbers
        const simde__m256i shuffled_im = simde_mm256_shuffle_epi32(input, SIMDE_MM_SHUFFLE(3, 3, 2, 2));
        const simde__m256i x_im = simde_mm256_srai_epi32(simde_mm256_madd_epi16(shuffled_im, alpha_256), output_shift);

        // Pack results and store (use storeu if unaligned memory is possible)
        const simde__m256i result = simde_mm256_packs_epi32(x_re, x_im);
        //simde_mm256_storeu_si256(&y_256[i], result);
        //simde_mm256_store_si256(&y_256[i], result);
        y_256[i] = result;
    }
#else // defined(AVX2_PROTOTYPE)

    typedef uint32_t aliasing_uint32_t __attribute__((may_alias));
    const c16_t for_re = {alpha->r, -alpha->i};
    const simde__m256i alpha_for_real = simde_mm256_set1_epi32(*(aliasing_uint32_t *)&for_re);
    const c16_t for_im = {alpha->i, alpha->r};
    const simde__m256i alpha_for_imag = simde_mm256_set1_epi32(*(aliasing_uint32_t *)&for_im);

#if defined(AVX2_FASTFAST)
    const simde__m256i perm_mask = simde_mm256_set_epi8(31,
                                                        30,
                                                        23,
                                                        22,
                                                        29,
                                                        28,
                                                        21,
                                                        20,
                                                        27,
                                                        26,
                                                        19,
                                                        18,
                                                        25,
                                                        24,
                                                        17,
                                                        16,
                                                        15,
                                                        14,
                                                        7,
                                                        6,
                                                        13,
                                                        12,
                                                        5,
                                                        4,
                                                        11,
                                                        10,
                                                        3,
                                                        2,
                                                        9,
                                                        8,
                                                        1,
                                                        0);
#endif

    // Loop over SIMD-sized chunks (8 complex numbers per iteration)
/*
    if (N % (12*8))
        printf("%d is not divisible by %d.\n", N, 12*8);
    // unroll by 12
    #if defined(__clang__)
      #pragma clang loop unroll_count(12)
    #elif defined(__GNUC__)
      #pragma GCC unroll 12
    #endif
*/
    for (uint32_t i = 0; i < (N >> 3); i++) {
      // Load input vector from x_256 (use loadu/lddqu if unaligned memory is possible)
      //simde__m256i input = simde_mm256_loadu_si256(&x_256[i]);
      //simde__m256i input = simde_mm256_lddqu_si256(&x_256[i]);
      //simde__m256i input = simde_mm256_load_si256(&x_256[i]);
      const simde__m256i x256 = x_256[i];

      const simde__m256i x_re = simde_mm256_srai_epi32(simde_mm256_madd_epi16(x256, alpha_for_real), output_shift);
      const simde__m256i x_im = simde_mm256_srai_epi32(simde_mm256_madd_epi16(x256, alpha_for_imag), output_shift);

      const simde__m256i x_re_im = simde_mm256_packs_epi32(x_re, x_im);
#if defined(AVX2_FASTFAST)
      // a bit faster than unpacklo+unpackhi with packs or blend
      const simde__m256i result  = simde_mm256_shuffle_epi8(x_re_im, perm_mask);
#else
     // Perform interleaving of real and imaginary parts
    simde__m256i real = simde_mm256_unpacklo_epi16(x_re_im, x_re_im);
    simde__m256i imag = simde_mm256_unpackhi_epi16(x_re_im, x_re_im);
    // simde_mm256_blend_epi16 with blend mask 10101010
    simde__m256i result = simde_mm256_blend_epi16(real, imag, 0xAA);
    // or interleave using simde_mm256_packs_epi16 (not working!!!)
    //simde__m256i result = simde_mm256_packs_epi16(real, imag);
#endif
      // Pack results and store (use storeu if unaligned memory is possible)
      //simde_mm256_storeu_si256(&y_256[i], result);
      //simde_mm256_store_si256(&y_256[i], result);
      y_256[i] = result;
    }
#endif // defined(AVX2_PROTOTYPE)
#endif // defined(AVX2_FALLBACK)
}
#endif // defined(__x86_64__) || defined (__i386__)

#if defined(__x86_64__) || defined (__i386__)
#if defined(AVX2_FALLBACK)
__attribute__((optimize("Ofast"), target("avx2")))
#endif // defined(AVX2_FALLBACK)
#endif // defined(__x86_64__) || defined (__i386__)
void rotate_cpx_vector(const c16_t *const x, const c16_t *const alpha, c16_t *y, uint32_t N, uint16_t output_shift)
{

  //printf("The N value is: %" PRIu32 "\n", N);
  // multiply a complex vector with a complex value (alpha)
  // stores result in y
  // N is the number of complex numbers
  // output_shift reduces the result of the multiplication by this number of bits
#if defined(__x86_64__) || defined (__i386__)
  if ( __builtin_cpu_supports("avx2")) {

    #define N_AVX2 8 // processes in blocks of 8
    #define N_SSE  4 // processes in blocks of 4

    // Check aligned with same offset for 32 byte (check for 32 byte includes 16 byte)
    bool is_x_y_aligned_32 = (((uintptr_t)x | (uintptr_t)y) % 32 == 0);
    bool full_avx2_processing = (N % N_AVX2 == 0);
    bool full_sse_processing  = (N % N_SSE  == 0);

    // Choose processing method
    if (full_avx2_processing && is_x_y_aligned_32) {
      // Case 1: Aligned to 16 and 32 bytes, N divisible by 8
      rotate_cpx_vector_avx2(x, alpha, y, N, output_shift);
    }
    else if (full_sse_processing) {
      //uint32_t offset_sse  = is_x_y_aligned_32 ? N - N_SSE : 0;     //? Case2 : Case3
      //uint32_t offset_avx2 = is_x_y_aligned_32 ? 0         : N_SSE; //? Case2 : Case3
      
      uint32_t number_avx2 = 3 * N_SSE; // process N=12 with SSE
      uint32_t number_sse =  N - number_avx2;

      uint32_t offset_avx2 = number_avx2 * !is_x_y_aligned_32;
      uint32_t offset_sse  = number_sse  *  is_x_y_aligned_32;
      // Case 2: N divisible by 4 (SSE tail processing) and aligned to 16 and 32 bytes,
      // Case 3: N divisible by 4 (SSE head processing) and aligned to 16 but not 32 bytes,
      rotate_cpx_vector_sse (x + offset_sse,  alpha, y + offset_sse,  number_avx2, output_shift);
      rotate_cpx_vector_avx2(x + offset_avx2, alpha, y + offset_avx2, number_sse,  output_shift);

    }
/*
    else
    {
        printf("rotate_cpx_vector: Ups, check alignment of x and y input buffer!\n");
    }
*/
  } else {
#endif // defined(__x86_64__) || defined (__i386__)
    rotate_cpx_vector_sse(x, alpha, y, N, output_shift);
#if defined(__x86_64__) || defined (__i386__)
  }
#endif // defined(__x86_64__) || defined (__i386__)
}

#ifdef MAIN
#define L 8

main ()
{

  int16_t input[256] __attribute__((aligned(16)));
  int16_t input2[256] __attribute__((aligned(16)));
  int16_t output[256] __attribute__((aligned(16)));
  c16_t alpha;

  int i;

  input[0] = 100;
  input[1] = 200;
  input[2] = -200;
  input[3] = 100;
  input[4] = 1000;
  input[5] = 2000;
  input[6] = -2000;
  input[7] = 1000;
  input[8] = 100;
  input[9] = 200;
  input[10] = -200;
  input[11] = 100;
  input[12] = 1000;
  input[13] = 2000;
  input[14] = -2000;
  input[15] = 1000;

  input2[0] = 1;
  input2[1] = 2;
  input2[2] = 1;
  input2[3] = 2;
  input2[4] = 10;
  input2[5] = 20;
  input2[6] = 10;
  input2[7] = 20;
  input2[8] = 1;
  input2[9] = 2;
  input2[10] = 1;
  input2[11] = 2;
  input2[12] = 1000;
  input2[13] = 2000;
  input2[14] = 1000;
  input2[15] = 2000;

  alpha->r=32767;
  alpha->i=0;

  //mult_cpx_vector(input,input2,output,L,0);
  rotate_cpx_vector_norep(input,alpha,input,L,15);

  for(i=0; i<L<<1; i++)
    printf("output[i]=%d\n",input[i]);

}

#endif //MAIN


