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
  uint8_t tmp[8][68 * 384] __attribute__((aligned(32)));
  uint8_t *d[impp->n_segments];
  for (int rr = impp->macro_num * 8, i = 0; rr < impp->n_segments && rr < (impp->macro_num + 1) * 8; rr++, i++)
    d[rr] = tmp[i];

  uint8_t *c[nrLDPC_TB_encoding_parameters->C];
  for (int r = 0; r < nrLDPC_TB_encoding_parameters->C; r++)
    c[r] = nrLDPC_TB_encoding_parameters->segments[r].c;
  start_meas(&nrLDPC_TB_encoding_parameters->segments[impp->macro_num * 8].ts_ldpc_encode);
  LDPCencoder(c, d, impp);
  stop_meas(&nrLDPC_TB_encoding_parameters->segments[impp->macro_num * 8].ts_ldpc_encode);
  // Compute where to place in output buffer that is concatenation of all segments

  uint32_t r_offset = 0;
  for (int i = 0; i < impp->macro_num * 8; i++)
    r_offset += nrLDPC_TB_encoding_parameters->segments[i].E;
  for (int rr = impp->macro_num * 8; rr < impp->n_segments && rr < (impp->macro_num + 1) * 8; rr++) {
    if (impp->F > 0) {
      // writing into positions d[r][k-2Zc] as in clause 5.3.2 step 2) in 38.212
      memset(&d[rr][impp->K - impp->F - 2 * impp->Zc], NR_NULL, impp->F);
    }

#ifdef DEBUG_LDPC_ENCODING
    LOG_D(PHY, "rvidx in encoding = %d\n", rel15->rvIndex[0]);
#endif
    uint32_t E = nrLDPC_TB_encoding_parameters->segments[rr].E;
    LOG_D(NR_PHY,
          "Rate Matching, Code segment %d/%d (coded bits (G) %u, E %d, Filler bits %d, Filler offset %d mod_order %d, nb_rb "
          "%d,nrOfLayer %d)...\n",
          rr,
          impp->n_segments,
          G,
          E,
          impp->F,
          impp->K - impp->F - 2 * impp->Zc,
          mod_order,
          nb_rb,
          nrLDPC_TB_encoding_parameters->nb_layers);

    uint32_t Tbslbrm = nrLDPC_TB_encoding_parameters->tbslbrm;

    uint8_t e[E];
    bzero(e, E);
    start_meas(&nrLDPC_TB_encoding_parameters->segments[rr].ts_rate_match);
    nr_rate_matching_ldpc(Tbslbrm,
                          impp->BG,
                          impp->Zc,
                          d[rr],
                          e,
                          impp->n_segments,
                          impp->F,
                          impp->K - impp->F - 2 * impp->Zc,
                          nrLDPC_TB_encoding_parameters->rv_index,
                          E);
    stop_meas(&nrLDPC_TB_encoding_parameters->segments[rr].ts_rate_match);
    if (impp->K - impp->F - 2 * impp->Zc > E) {
      LOG_E(PHY, "dlsch coding A %d K %d G %d (nb_rb %d, mod_order %d)\n", A, impp->K, G, nb_rb, (int)mod_order);

      LOG_E(NR_PHY,
            "Rate Matching, Code segment %d/%d (coded bits (G) %u, E %d, K %d, Filler bits %d, Filler offset %d mod_order %d, "
            "nb_rb %d)...\n",
            rr,
            impp->n_segments,
            G,
            E,
            impp->K,
            impp->F,
            impp->K - impp->F - 2 * impp->Zc,
            mod_order,
            nb_rb);
    }
#ifdef DEBUG_LDPC_ENCODING

    for (int i = 0; i < 16; i++)
      printf("output ratematching e[%d]= %d r_offset %u\n", i, e[i], r_offset);

#endif
    start_meas(&nrLDPC_TB_encoding_parameters->segments[rr].ts_interleave);
    nr_interleaving_ldpc(E, mod_order, e, impp->output + r_offset);
    stop_meas(&nrLDPC_TB_encoding_parameters->segments[rr].ts_interleave);
#ifdef DEBUG_LDPC_ENCODING

    for (int i = 0; i < 16; i++)
      printf("output interleaving f[%d]= %d r_offset %u\n", i, impp->output[i + r_offset], r_offset);

    if (r == impp->n_segments - 1)
      write_output("enc_output.m", "enc", impp->output, G, 1, 4);

#endif
    r_offset += E;
  }

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
