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

void mult_cpx_conj_vector(int16_t *x1,
                         int16_t *x2,
                         int16_t *y,
                         uint32_t N,
                         int output_shift,
			                   int madd)
{
  // Multiply elementwise the complex conjugate of x1 with x2.
  // x1       - input 1    in the format  |Re0 Im0 Re1 Im1|,......,|Re(N-2)  Im(N-2) Re(N-1) Im(N-1)|
  //            We assume x1 with a dinamic of 15 bit maximum
  //
  // x2       - input 2    in the format  |Re0 Im0 Re1 Im1|,......,|Re(N-2)  Im(N-2) Re(N-1) Im(N-1)|
  //            We assume x2 with a dinamic of 14 bit maximum
  ///
  // y        - output     in the format  |Re0 Im0 Re1 Im1|,......,|Re(N-2)  Im(N-2) Re(N-1) Im(N-1)|
  //
  // N        - the size f the vectors (this function does N cpx mpy. WARNING: N>=4;
  //
  // output_shift  - shift to be applied to generate output
  //
  // madd - add the output to y

  simde__m128i *x1_128 = (simde__m128i *)x1;
  simde__m128i *x2_128 = (simde__m128i *)x2;
  simde__m128i *y_128  = (simde__m128i *)y;

  // SSE compute 4 cpx multiply for each loop
  for(uint32_t i = 0; i < (N >> 2); i++) {
    simde__m128i result = oai_mm_cpx_mult_conja(x1_128[i], x2_128[i], output_shift);
    
    if (madd==1)
      result = simde_mm_adds_epi16(y_128[i], result);

    y_128[i] = result;
  }
  simde_mm_empty();
  simde_m_empty();

}

void mult_cpx_vector(int16_t *x1, //Q15
                     int16_t *x2, //Q13
                     int16_t *y,
                     uint32_t N,
                     int output_shift)
{
  // Multiply elementwise x1 with x2.
  // x1       - input 1    in the format  |Re0 Im0 Re1 Im1|,......,|Re(N-2)  Im(N-2) Re(N-1) Im(N-1)|
  //            We assume x1 with a dinamic of 15 bit maximum
  //
  // x2       - input 2    in the format  |Re0 Im0 Re1 Im1|,......,|Re(N-2)  Im(N-2) Re(N-1) Im(N-1)|
  //            We assume x2 with a dinamic of 14 bit maximum
  ///
  // y        - output     in the format  |Re0 Im0 Re1 Im1|,......,|Re(N-2)  Im(N-2) Re(N-1) Im(N-1)|
  //
  // N        - the size f the vectors (this function does N cpx mpy. WARNING: N>=4;
  //
  // output_shift  - shift to be applied to generate output

  simde__m128i *x1_128 = (simde__m128i *)x1;
  simde__m128i *x2_128 = (simde__m128i *)x2;
  simde__m128i *y_128  = (simde__m128i *)y;

  //right shift by 13 while p_a * x0 and 15 while
  // SSE compute 4 cpx multiply for each loop
  for(uint32_t i=0; i < (N >> 2); i++) {
    y_128[i] = oai_mm_cpx_mult(x1_128[i], x2_128[i], output_shift);
  }
  simde_mm_empty();
  simde_m_empty();
  
}

void multadd_cpx_vector(int16_t *x1,
                        int16_t *x2,
                        int16_t *y,
                        uint8_t zero_flag,
                        uint32_t N,
                        int output_shift)
{
  // Multiply elementwise the complex conjugate of x1 with x2.
  // x1       - input 1    in the format  |Re0 Im0 Re1 Im1|,......,|Re(N-2)  Im(N-2) Re(N-1) Im(N-1)|
  //            We assume x1 with a dinamic of 15 bit maximum
  //
  // x2       - input 2    in the format  |Re0 Im0 Re1 Im1|,......,|Re(N-2)  Im(N-2) Re(N-1) Im(N-1)|
  //            We assume x2 with a dinamic of 14 bit maximum
  ///
  // y        - output     in the format  |Re0 Im0 Re1 Im1|,......,|Re(N-2)  Im(N-2) Re(N-1) Im(N-1)|
  //
  // zero_flag - Set output (y) to zero prior to disable accumulation
  //
  // N        - the size f the vectors (this function does N cpx mpy. WARNING: N>=4;
  //
  // output_shift  - shift to be applied to generate output

  simde__m128i *x1_128 = (simde__m128i *)x1;
  simde__m128i *x2_128 = (simde__m128i *)x2;
  simde__m128i *y_128  = (simde__m128i *)y;

  // SSE compute 4 cpx multiply for each loop
  for(uint32_t i = 0; i < (N >> 2); i++) {
    simde__m128i result = oai_mm_cpx_mult(x1_128[i], x2_128[i], output_shift);
    
    if (zero_flag == 0)
      result = simde_mm_adds_epi16(y_128[i], result);

    y_128[i] = result;
  }
  simde_mm_empty();
  simde_m_empty();

}
