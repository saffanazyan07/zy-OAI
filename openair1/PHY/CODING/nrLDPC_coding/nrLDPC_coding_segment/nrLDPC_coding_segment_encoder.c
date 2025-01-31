/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.0  (the "License"); you may not use this file
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

/*! \file PHY/CODING/nrLDPC_coding/nrLDPC_coding_segment/nrLDPC_coding_segment_encoder.c
 * \brief Top-level routines for implementing LDPC encoding of transport channels
 */

#include "nr_rate_matching.h"
#include "PHY/defs_gNB.h"
#include "PHY/CODING/coding_extern.h"
#include "PHY/CODING/coding_defs.h"
#include "PHY/CODING/lte_interleaver_inline.h"
#include "PHY/CODING/nrLDPC_coding/nrLDPC_coding_interface.h"
#include "PHY/CODING/nrLDPC_extern.h"
#include "PHY/NR_TRANSPORT/nr_transport_proto.h"
#include "PHY/NR_TRANSPORT/nr_transport_common_proto.h"
#include "PHY/NR_TRANSPORT/nr_dlsch.h"
#include "SCHED_NR/sched_nr.h"
#include "common/utils/LOG/vcd_signal_dumper.h"
#include "common/utils/LOG/log.h"
#include "common/utils/nr/nr_common.h"
#include <openair2/UTIL/OPT/opt.h>

#include <syscall.h>

// #define DEBUG_LDPC_ENCODING
// #define DEBUG_LDPC_ENCODING_FREE 1

extern ldpc_interface_t ldpc_interface_segment;

typedef struct ldpc8blocks_args_s {
  nrLDPC_TB_encoding_parameters_t *nrLDPC_TB_encoding_parameters;
  encoder_implemparams_t impp;
} ldpc8blocks_args_t;

static void ldpc8blocks_coding_segment(void *p)
{
  ldpc8blocks_args_t *args = (ldpc8blocks_args_t *)p;
  nrLDPC_TB_encoding_parameters_t *nrLDPC_TB_encoding_parameters = args->nrLDPC_TB_encoding_parameters;
  encoder_implemparams_t *impp = &args->impp;

  uint8_t mod_order = nrLDPC_TB_encoding_parameters->Qm;
  uint16_t nb_rb = nrLDPC_TB_encoding_parameters->nb_rb;
  uint32_t A = nrLDPC_TB_encoding_parameters->A;

  unsigned int G = nrLDPC_TB_encoding_parameters->G;
  LOG_D(PHY, "dlsch coding A %d K %d G %d (nb_rb %d, mod_order %d)\n", A, impp->K, G, nb_rb, (int)mod_order);

  // nrLDPC_encoder output is in "d"
  // let's make this interface happy!
  uint8_t d[68 * 384]__attribute__((aligned(64)));
  uint8_t *dp[2];
  dp[0]=&d[0];
  uint8_t *c[nrLDPC_TB_encoding_parameters->C];
  unsigned int macro_segment, macro_segment_end;

  
  macro_segment = 8*impp->macro_num;
  macro_segment_end = (impp->n_segments > 8*(impp->macro_num+1)) ? 8*(impp->macro_num+1) : impp->n_segments;
  for (int r = 0; r < nrLDPC_TB_encoding_parameters->C; r++)
    c[r]=nrLDPC_TB_encoding_parameters->segments[r].c;
  start_meas(&nrLDPC_TB_encoding_parameters->segments[impp->macro_num*8].ts_ldpc_encode);
  ldpc_interface_segment.LDPCencoder(c, dp, impp);
  stop_meas(&nrLDPC_TB_encoding_parameters->segments[impp->macro_num*8].ts_ldpc_encode);
  // Compute where to place in output buffer that is concatenation of all segments


  if (impp->F>0) {
      // writing into positions d[k-2Zc] as in clause 5.3.2 step 2) in 38.212
      memset(&d[impp->K - impp->F - 2 * impp->Zc], NR_NULL, impp->F);
  }

#ifdef DEBUG_LDPC_ENCODING
  LOG_D(PHY,"rvidx in encoding = %d\n", nrLDPC_TB_encoding_parameters->rv_index);
#endif
  const uint32_t E = nrLDPC_TB_encoding_parameters->segments[macro_segment].E;
  uint32_t E2=E,E2_first_segment=macro_segment_end-macro_segment;
  bool Eshift=false;
  for (int s=macro_segment;s<macro_segment_end;s++)
      if (nrLDPC_TB_encoding_parameters->segments[s].E != E) {
	 E2=nrLDPC_TB_encoding_parameters->segments[s].E;
         Eshift=true;
	 E2_first_segment = s-macro_segment;
         break;
      }	 
    

  LOG_D(NR_PHY,
        "Rate Matching, Code segment %d...%d/%d (coded bits (G) %u, E %d, E2 %d Filler bits %d, Filler offset %d mod_order %d, nb_rb "
          "%d,nrOfLayer %d)...\n",
        macro_segment,
        macro_segment_end,
        impp->n_segments,
        G,
        E,E2,
        impp->F,
        impp->K - impp->F - 2 * impp->Zc,
        mod_order,
        nb_rb,
        nrLDPC_TB_encoding_parameters->nb_layers);
/*
  printf("Rate Matching, Code segment %d...%d/%d (coded bits (G) %u, E %d, E2 %d Filler bits %d, Filler offset %d mod_order %d, nb_rb "
          "%d,nrOfLayer %d)...\n",
        macro_segment,
        macro_segment_end,
        impp->n_segments,
        G,
        E,E2,
        impp->F,
        impp->K - impp->F - 2 * impp->Zc,
        mod_order,
        nb_rb,
        nrLDPC_TB_encoding_parameters->nb_layers);
*/
  uint32_t Tbslbrm = nrLDPC_TB_encoding_parameters->tbslbrm;

  uint8_t e[E]__attribute__((aligned(64)));
  uint8_t f[E+64]__attribute__((aligned(64)));
  uint8_t e2[E2]__attribute__((aligned(64)));
  uint8_t f2[E2+64]__attribute__((aligned(64)));
  bzero (e, E);
  if (Eshift) bzero (e2,E2);
  start_meas(&nrLDPC_TB_encoding_parameters->segments[macro_segment].ts_rate_match);
  nr_rate_matching_ldpc(Tbslbrm,
                        impp->BG,
                        impp->Zc,
                        d,
                        e,
                        impp->n_segments,
                        impp->F,
                        impp->K - impp->F - 2 * impp->Zc,
                        nrLDPC_TB_encoding_parameters->rv_index,
                        E);
  if (Eshift)
    nr_rate_matching_ldpc(Tbslbrm,
                          impp->BG,
                          impp->Zc,
                          d,
                          e2,
                          impp->n_segments,
                          impp->F,
                          impp->K - impp->F - 2 * impp->Zc,
                          nrLDPC_TB_encoding_parameters->rv_index,
                          E2);

  stop_meas(&nrLDPC_TB_encoding_parameters->segments[macro_segment].ts_rate_match);
  if (impp->K - impp->F - 2 * impp->Zc > E) {
    LOG_E(PHY,
          "dlsch coding A %d  Kr %d G %d (nb_rb %d, mod_order %d)\n",
          A,
          impp->K,
          G,
          nb_rb,
          (int)mod_order);

    LOG_E(NR_PHY,
          "Rate Matching, Code segments %d...%d/%d (coded bits (G) %u, E %d, Kr %d, Filler bits %d, Filler offset %d mod_order %d, "
          "nb_rb %d)...\n",
          macro_segment,
	  macro_segment_end,
          impp->n_segments,
          G,
          E,
          impp->K,
          impp->F,
          impp->K - impp->F - 2 * impp->Zc,
          mod_order,
          nb_rb);
  }
  start_meas(&nrLDPC_TB_encoding_parameters->segments[macro_segment].ts_interleave);
  nr_interleaving_ldpc(E,
                       mod_order,
                       e,
                       f);
  if (Eshift)
    nr_interleaving_ldpc(E2,
                         mod_order,
                         e2,
                         f2);
  stop_meas(&nrLDPC_TB_encoding_parameters->segments[macro_segment].ts_interleave);
  if(impp->toutput != NULL) start_meas(impp->toutput);
/*
  for (int i=0;i<16;i++)
     for (int s=0;s<macro_segment_end-macro_segment;s++) 
        printf("i %d: segment %d : f[%d] %d\n",i,macro_segment+s,i,(f[i]>>s)&1);
*/
  // information part and puncture columns
 
  uint32_t Eoffset=0;
  for (int s=0; s<macro_segment; s++)
    Eoffset += (nrLDPC_TB_encoding_parameters->segments[s].E); 
#ifdef __AVX512VBMI__
  uint64_t *output_p = (uint64_t*)impp->output;
  int i=0;
  __m512i bitperm[8];
  uint32_t Eoffset2[8];
  uint32_t Eoffset2_bit[8];
  bitperm[0] = _mm512_set1_epi64(0x3830282018100800);
  __m512i inc   = _mm512_set1_epi8(0x1);
  Eoffset2[0] = Eoffset>>6;
  Eoffset2_bit[0] = Eoffset&63;
  //printf("E2_first_segment %d,Eoffset %d, Eoffset2 %d, Eoffset2_bit %d\n",E2_first_segment,Eoffset,Eoffset2[0],Eoffset2_bit[0]);
  for (int n=1;n<E2_first_segment;n++) {
    bitperm[n]   = _mm512_add_epi8(bitperm[n-1],inc);
    Eoffset += E;
    Eoffset2[n]  = Eoffset>>6;
    Eoffset2_bit[n] = Eoffset&63;
    //printf("E %d : n %d, Eoffset %d, Eoffset2 %d, Eoffset2_bit %d\n",E,n,Eoffset,Eoffset2[n],Eoffset2_bit[n]);
  } 
  if (Eshift) {
    bitperm[E2_first_segment]   = _mm512_add_epi8(bitperm[E2_first_segment-1],inc);
    Eoffset += E;
    Eoffset2[E2_first_segment] = Eoffset>>6;
    Eoffset2_bit[E2_first_segment] = Eoffset &63;
    //printf("E2 %d : Eoffset %d, Eoffset2 %d, Eoffset2_bit %d\n",E2,Eoffset,Eoffset2[E2_first_segment],Eoffset2_bit[E2_first_segment]);
  }
  for (int n=E2_first_segment+1;n<macro_segment_end-macro_segment;n++) {
    bitperm[n]   = _mm512_add_epi8(bitperm[n-1],inc);
    Eoffset+=E2;
    Eoffset2[n] = Eoffset>>6;
    Eoffset2_bit[n] = Eoffset&63;
    //printf("E2 %d (macro_segment_end %d, macro_segment %d) : n %d, Eoffset %d, Eoffset2 %d, Eoffset2_bit %d\n",E2,macro_segment_end,macro_segment,n,Eoffset,Eoffset2[n],Eoffset2_bit[n]);
  }
  int i2=0;
  __m64 tmp;
  for (i=0;i<E2;i+=64,i2++) {
     if (i<E) {
      for (int j=0; j < E2_first_segment; j++) {
#ifdef DEBUG_BIT_INTERLEAVE
        printf("segment %d : qword %d, first bit %d last bit %d\n",j, 
            Eoffset2[j]+(i>>6),
            (Eoffset2_bit[j] + i)&63,
            (i<=(E-64)) ? 63 : (Eoffset2_bit[j] + E-1)&63);
        if  (Eoffset2_bit[j] > 0) 
          printf("segment %d : qword %d, first bit %d last bit %d\n",j,
                 1+Eoffset2[j]+(i>>6),
                 0,
                 ((Eoffset2_bit[j] + i)&63) - 1);
#endif 
        // Note: Here and below for AVX2, we are using the 64-bit SIMD instruction
        // instead of C >>/<< because when the Eoffset2_bit is 64 or 0, the <<
        // and >> operations are undefined and in fact don't give "0" which is
        // what we want here. The SIMD version do give 0 when the shift is 64
        tmp = (__m64)_mm512_bitshuffle_epi64_mask(((__m512i *)f)[i2],bitperm[j]);
        *(__m64*)(output_p + Eoffset2[j])   = _mm_or_si64(*(__m64*)(output_p + Eoffset2[j]),_mm_slli_si64(tmp,Eoffset2_bit[j]));
        *(__m64*)(output_p + Eoffset2[j]+1) = _mm_or_si64(*(__m64*)(output_p + Eoffset2[j]+1),_mm_srli_si64(tmp,(64-Eoffset2_bit[j])));
#ifdef DEBUG_BIT_INTERLEAVE
       if (j<=1) { 
           printf("pos : %d, i2 %d, j %d\n",(int)(output_p + Eoffset2[j]-(uint64_t*)impp->output),i2,j);
           printf("i : %x.%x.%x.%x.%x.%x.%x.%x\n",
           ((uint8_t*)(output_p + Eoffset2[j]))[0],
           ((uint8_t*)(output_p + Eoffset2[j]))[1],
           ((uint8_t*)(output_p + Eoffset2[j]))[2],
           ((uint8_t*)(output_p + Eoffset2[j]))[3],
           ((uint8_t*)(output_p + Eoffset2[j]))[4],
           ((uint8_t*)(output_p + Eoffset2[j]))[5],
           ((uint8_t*)(output_p + Eoffset2[j]))[6],
           ((uint8_t*)(output_p + Eoffset2[j]))[7]);
           printf("i+1 : %x.%x.%x.%x.%x.%x.%x.%x\n",
           ((uint8_t*)(output_p + Eoffset2[j]+1))[0],
           ((uint8_t*)(output_p + Eoffset2[j]+1))[1],
           ((uint8_t*)(output_p + Eoffset2[j]+1))[2],
           ((uint8_t*)(output_p + Eoffset2[j]+1))[3],
           ((uint8_t*)(output_p + Eoffset2[j]+1))[4],
           ((uint8_t*)(output_p + Eoffset2[j]+1))[5],
           ((uint8_t*)(output_p + Eoffset2[j]+1))[6],
           ((uint8_t*)(output_p + Eoffset2[j]+1))[7]);
           printf("%x.%x.%x.%x.%x.%x.%x.%x\n",
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],0),0)<<(0-j))&1)+
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],0),1)<<(1-j))&2)+
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],0),2)<<(2-j))&4)+
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],0),3)<<(3-j))&8)+
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],0),4)<<(4-j))&16)+
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],0),5)<<(5-j))&32)+
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],0),6)<<(6-j))&64)+
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],0),7)<<(7-j))&128),
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],0),8)<<(0-j))&1)+
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],0),9)<<(1-j))&2)+
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],0),10)<<(2-j))&4)+
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],0),11)<<(3-j))&8)+
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],0),12)<<(4-j))&16)+
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],0),13)<<(5-j))&32)+
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],0),14)<<(6-j))&64)+
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],0),15)<<(7-j))&128),
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],0),16)<<(0-j))&1)+
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],0),17)<<(1-j))&2)+
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],0),18)<<(2-j))&4)+
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],0),19)<<(3-j))&8)+
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],0),20)<<(4-j))&16)+
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],0),21)<<(5-j))&32)+
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],0),22)<<(6-j))&64)+
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],0),23)<<(7-j))&128),
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],0),24)<<(0-j))&1)+
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],0),25)<<(1-j))&2)+
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],0),26)<<(2-j))&4)+
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],0),27)<<(3-j))&8)+
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],0),28)<<(4-j))&16)+
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],0),29)<<(5-j))&32)+
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],0),30)<<(6-j))&64)+
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],0),31)<<(7-j))&128),
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],1),0)<<(0-j))&1)+
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],1),1)<<(1-j))&2)+
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],1),2)<<(2-j))&4)+
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],1),3)<<(3-j))&8)+
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],1),4)<<(4-j))&16)+
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],1),5)<<(5-j))&32)+
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],1),6)<<(6-j))&64)+
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],1),7)<<(7-j))&128),
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],1),8)<<(0-j))&1)+
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],1),9)<<(1-j))&2)+
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],1),10)<<(2-j))&4)+
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],1),11)<<(3-j))&8)+
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],1),12)<<(4-j))&16)+
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],1),13)<<(5-j))&32)+
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],1),14)<<(6-j))&64)+
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],1),15)<<(7-j))&128),
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],1),16)<<(0-j))&1)+
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],1),17)<<(1-j))&2)+
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],1),18)<<(2-j))&4)+
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],1),19)<<(3-j))&8)+
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],1),20)<<(4-j))&16)+
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],1),21)<<(5-j))&32)+
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],1),22)<<(6-j))&64)+
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],1),23)<<(7-j))&128),
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],1),24)<<(0-j))&1)+
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],1),25)<<(1-j))&2)+
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],1),26)<<(2-j))&4)+
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],1),27)<<(3-j))&8)+
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],1),28)<<(4-j))&16)+
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],1),29)<<(5-j))&32)+
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],1),30)<<(6-j))&64)+
               ((_mm256_extract_epi8(_mm512_extracti32x8_epi32(((__m512i *)f)[i2],1),31)<<(7-j))&128));
       }
#endif
      } //for (int j=0;
     } // if (i < E)
     for (int j=E2_first_segment; j < macro_segment_end-macro_segment; j++) {
#ifdef DEBUG_BIT_INTERLEAVE
       printf("segment %d : qword %d, first bit %d last bit %d\n",j,
           Eoffset2[j]+(i>>6),
           (Eoffset2_bit[j] + i)&63,
           (i<=(E-64)) ? 63 : (Eoffset2_bit[j] + E-1)&63);
       if (Eoffset2_bit[j] > 0) 
         printf("segment %d : qword %d, first bit %d last bit %d\n",j,
                1+Eoffset2[j]+(i>>6),
                0,
                ((Eoffset2_bit[j] + i)&63) - 1);
#endif
       tmp = (__m64)_mm512_bitshuffle_epi64_mask(((__m512i *)f2)[i2],bitperm[j]);
       *(__m64*)(output_p + Eoffset2[j])   = _mm_or_si64(*(__m64*)(output_p + Eoffset2[j]),
                                                          _mm_slli_si64(tmp,Eoffset2_bit[j]));
       *(__m64*)(output_p + Eoffset2[j]+1) = _mm_or_si64(*(__m64*)(output_p + Eoffset2[j]+1),_mm_srli_si64(tmp,(64-Eoffset2_bit[j])));
     }
     output_p++;
  }

#elif defined(__aarch64__)
  uint16_t *output_p = (uint16_t*)impp->output;
  int i=0;
  uint32_t Eoffset2[8];
  uint32_t Eoffset2_bit[8];
  Eoffset2[0] = Eoffset>>4;
  Eoffset2_bit[0] = Eoffset&15;
  //printf("E2_first_segment %d,Eoffset %d, Eoffset2 %d, Eoffset2_bit %d\n",E2_first_segment,Eoffset,Eoffset2[0],Eoffset2_bit[0]);
  for (int n=1;n<E2_first_segment;n++) {
    Eoffset += E;
    Eoffset2[n]  = Eoffset>>4;
    Eoffset2_bit[n] = Eoffset&15;
    //printf("E %d : n %d, Eoffset %d, Eoffset2 %d, Eoffset2_bit %d\n",E,n,Eoffset,Eoffset2[n],Eoffset2_bit[n]);
  } 
  if (Eshift) {
    Eoffset += E;
    Eoffset2[E2_first_segment] = Eoffset>>4;
    Eoffset2_bit[E2_first_segment] = Eoffset &15;
    //printf("E2 %d : Eoffset %d, Eoffset2 %d, Eoffset2_bit %d\n",E2,Eoffset,Eoffset2[E2_first_segment],Eoffset2_bit[E2_first_segment]);
  }
  for (int n=E2_first_segment+1;n<macro_segment_end-macro_segment;n++) {
    Eoffset+=E2;
    Eoffset2[n] = Eoffset>>4;
    Eoffset2_bit[n] = Eoffset&15;
    //printf("E2 %d (macro_segment_end %d, macro_segment %d) : n %d, Eoffset %d, Eoffset2 %d, Eoffset2_bit %d\n",E2,macro_segment_end,macro_segment,n,Eoffset,Eoffset2[n],Eoffset2_bit[n]);
  }
  int i2=0;
  const int8_t __attribute__ ((aligned (16))) ucShift[8][16] = {
    {0,1,2,3,4,5,6,7,0,1,2,3,4,5,6,7},     // segment 0
    {-1,0,1,2,3,4,5,6,-1,0,1,2,3,4,5,6},   // segment 1
    {-2,-1,0,1,2,3,4,5,-2,-1,0,1,2,3,4,5}, // segment 2
    {-3,-2,-1,0,1,2,3,4,-3,-2,-1,0,1,2,3,4}, // segment 3
    {-4,-3,-2,-1,0,1,2,3,-4,-3,-2,-1,0,1,2,3}, // segment 4
    {-5,-4,-3,-2,-1,0,1,2,-5,-4,-3,-2,-1,0,1,2}, // segment 5
    {-6,-5,-4,-3,-2,-1,0,1,-6,-5,-4,-3,-2,-1,0,1}, // segment 6
    {-7,-6,-5,-4,-3,-2,-1,0,-7,-6,-5,-4,-3,-2,-1,0}}; // segment 7
  const uint8_t __attribute__ ((aligned (16))) masks[16] = 
      {0x1,0x2,0x4,0x8,0x10,0x20,0x40,0x80,0x1,0x2,0x4,0x8,0x10,0x20,0x40,0x80};
  int8x16_t vshift[8];
  for (int n=0;n<8;n++) vshift[n] = vld1q_s8(ucShift[n]);
  uint8x16_t vmask  = vld1q_u8(masks);

  int32_t tmp;
  uint8x16_t cshift;
  for (i=0;i<E2;i+=16,i2++) {
     if (i<E) {	
       for (int j=0; j < E2_first_segment; j++) {
         cshift = vandq_u8(vshlq_u8(((uint8x16_t*)f)[i2],vshift[j]),vmask);
         tmp = (int)vaddv_u8(vget_low_u8(cshift));
         tmp += (int)(vaddv_u8(vget_high_u8(cshift))<<8);
         *(output_p + Eoffset2[j])   |= (uint16_t)(tmp<<Eoffset2_bit[j]);
         *(output_p + Eoffset2[j]+1) |= (uint16_t)(tmp>>(16-Eoffset2_bit[j]));
       }
     }
     for (int j=E2_first_segment; j < macro_segment_end-macro_segment; j++) {
       cshift = vandq_u8(vshlq_u8(((uint8x16_t*)f2)[i2],vshift[j]),vmask);
       tmp = (int)vaddv_u8(vget_low_u8(cshift));
       tmp += (int)(vaddv_u8(vget_high_u8(cshift))<<8);
       *(output_p + Eoffset2[j])   |= (uint16_t)(tmp<<Eoffset2_bit[j]);
       *(output_p + Eoffset2[j]+1) |= (uint16_t)(tmp>>(16-Eoffset2_bit[j]));
     }
     output_p++;
  }
       
#else
  uint32_t *output_p = (uint32_t*)impp->output;
  int i=0;
  uint32_t Eoffset2[8];
  uint32_t Eoffset2_bit[8];
  Eoffset2[0] = Eoffset>>5;
  Eoffset2_bit[0] = Eoffset&31;
  //printf("E2_first_segment %d,Eoffset %d, Eoffset2 %d, Eoffset2_bit %d\n",E2_first_segment,Eoffset,Eoffset2[0],Eoffset2_bit[0]);
  for (int n=1;n<E2_first_segment;n++) {
    Eoffset += E;
    Eoffset2[n]  = Eoffset>>5;
    Eoffset2_bit[n] = Eoffset&31;
    //printf("E %d : n %d, Eoffset %d, Eoffset2 %d, Eoffset2_bit %d\n",E,n,Eoffset,Eoffset2[n],Eoffset2_bit[n]);
  } 
  if (Eshift) {
    Eoffset += E;
    Eoffset2[E2_first_segment] = Eoffset>>5;
    Eoffset2_bit[E2_first_segment] = Eoffset &31;
    //printf("E2 %d : Eoffset %d, Eoffset2 %d, Eoffset2_bit %d\n",E2,Eoffset,Eoffset2[E2_first_segment],Eoffset2_bit[E2_first_segment]);
  }
  for (int n=E2_first_segment+1;n<macro_segment_end-macro_segment;n++) {
    Eoffset+=E2;
    Eoffset2[n] = Eoffset>>5;
    Eoffset2_bit[n] = Eoffset&31;
    //printf("E2 %d (macro_segment_end %d, macro_segment %d) : n %d, Eoffset %d, Eoffset2 %d, Eoffset2_bit %d\n",E2,macro_segment_end,macro_segment,n,Eoffset,Eoffset2[n],Eoffset2_bit[n]);
  }
  int i2=0;
  int tmp;
  __m64 tmp64,tmp64b,tmp64c;
  
  for (i=0;i<E2;i+=32,i2++) {
     if (i<E) {
       for (int j=0; j < E2_first_segment; j++) {
         // Note: Here and below, we are using the 64-bit SIMD instruction
         // instead of C >>/<< because when the Eoffset2_bit is 64 or 0, the <<
         // and >> operations are undefined and in fact don't give "0" which is
         // what we want here. The SIMD version do give 0 when the shift is 64
         tmp = _mm256_movemask_epi8(_mm256_slli_epi16(((__m256i *)f)[i2],7-j));
         tmp64=_mm_set1_pi32(tmp);
         tmp64b  = _mm_or_si64(*(__m64*)(output_p + Eoffset2[j]),_mm_slli_pi32(tmp64,Eoffset2_bit[j]));
         tmp64c = _mm_or_si64(*(__m64*)(output_p + Eoffset2[j]),_mm_srli_pi32(tmp64,(32-Eoffset2_bit[j])));
         *(output_p + Eoffset2[j])   = _m_to_int(tmp64b);
         *(output_p + Eoffset2[j]+1) = _m_to_int(_mm_srli_si64(tmp64c,32));
       }
    } 
    for (int j=E2_first_segment; j < macro_segment_end-macro_segment; j++) {
       tmp = _mm256_movemask_epi8(_mm256_slli_epi16(((__m256i *)f2)[i2],7-j));
       tmp64=_mm_set1_pi32(tmp);
       tmp64b  = _mm_or_si64(*(__m64*)(output_p + Eoffset2[j]),_mm_slli_pi32(tmp64,Eoffset2_bit[j]));
       tmp64c = _mm_or_si64(*(__m64*)(output_p + Eoffset2[j]),_mm_srli_pi32(tmp64,(32-Eoffset2_bit[j])));
       *(output_p + Eoffset2[j])   = _m_to_int(tmp64b);
       *(output_p + Eoffset2[j]+1) = _m_to_int(_mm_srli_si64(tmp64c,32));
    }
    output_p++;
  }

  
#endif

  if(impp->toutput != NULL) stop_meas(impp->toutput);
/*
  printf("E %d (mod16 %d) F %d\n",E,E&15,impp->F); 
  for (int i=0;i<16;i++)
     for (int s=0;s<macro_segment_end-macro_segment;s++) 
        printf("i %d: segment %d : output[%d] %d\n",i,macro_segment+s,i,output_offset[i+s*E]);
*/
  // Task running in // completed
  completed_task_ans(impp->ans);
}

static int nrLDPC_prepare_TB_encoding(nrLDPC_slot_encoding_parameters_t *nrLDPC_slot_encoding_parameters,
                                      int dlsch_id,
                                      thread_info_tm_t *t_info)
{
  nrLDPC_TB_encoding_parameters_t *nrLDPC_TB_encoding_parameters = &nrLDPC_slot_encoding_parameters->TBs[dlsch_id];

  encoder_implemparams_t impp = {0};

  impp.n_segments = nrLDPC_TB_encoding_parameters->C;
  impp.tinput = nrLDPC_slot_encoding_parameters->tinput;
  impp.tprep = nrLDPC_slot_encoding_parameters->tprep;
  impp.tparity = nrLDPC_slot_encoding_parameters->tparity;
  impp.toutput = nrLDPC_slot_encoding_parameters->toutput;
  impp.Kb = nrLDPC_TB_encoding_parameters->Kb;
  impp.Zc = nrLDPC_TB_encoding_parameters->Z;
  impp.BG = nrLDPC_TB_encoding_parameters->BG;
  impp.output = nrLDPC_TB_encoding_parameters->segments->output; 
  impp.K = nrLDPC_TB_encoding_parameters->K;
  impp.F = nrLDPC_TB_encoding_parameters->F;

  size_t const n_seg = (impp.n_segments / 8 + ((impp.n_segments & 7) == 0 ? 0 : 1));

  for (int j = 0; j < n_seg; j++) {
    ldpc8blocks_args_t *perJobImpp = &((ldpc8blocks_args_t *)t_info->buf)[t_info->len];
    DevAssert(t_info->len < t_info->cap);
    impp.ans = t_info->ans;
    t_info->len += 1;

    impp.macro_num = j;
    perJobImpp->impp = impp;
    perJobImpp->nrLDPC_TB_encoding_parameters = nrLDPC_TB_encoding_parameters;

    task_t t = {.func = ldpc8blocks_coding_segment, .args = perJobImpp};
    pushTpool(nrLDPC_slot_encoding_parameters->threadPool, t);
  }
  return n_seg;
}

int nrLDPC_coding_encoder(nrLDPC_slot_encoding_parameters_t *nrLDPC_slot_encoding_parameters)
{
  int nbTasks = 0;

  for (int dlsch_id = 0; dlsch_id < nrLDPC_slot_encoding_parameters->nb_TBs; dlsch_id++) {
    nrLDPC_TB_encoding_parameters_t *nrLDPC_TB_encoding_parameters = &nrLDPC_slot_encoding_parameters->TBs[dlsch_id];
    size_t n_seg = (nrLDPC_TB_encoding_parameters->C / 8 + ((nrLDPC_TB_encoding_parameters->C & 7) == 0 ? 0 : 1));
    nbTasks += n_seg;
  }
  ldpc8blocks_args_t arr[nbTasks];
  task_ans_t ans;
  init_task_ans(&ans, nbTasks);
  thread_info_tm_t t_info = {.buf = (uint8_t *)arr, .len = 0, .cap = nbTasks, .ans = &ans};

  int nbEncode = 0;
  for (int dlsch_id = 0; dlsch_id < nrLDPC_slot_encoding_parameters->nb_TBs; dlsch_id++) {
    nbEncode += nrLDPC_prepare_TB_encoding(nrLDPC_slot_encoding_parameters, dlsch_id, &t_info);
  }
  if (nbEncode < nbTasks) {
    completed_many_task_ans(&ans, nbTasks - nbEncode);
  }
  // Execute thread pool tasks
  join_task_ans(&ans);

  return 0;
}
