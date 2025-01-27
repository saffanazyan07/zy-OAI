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

#define _GNU_SOURCE // For pthread_setname_np
#include <pthread.h>
#include <openair1/PHY/impl_defs_top.h>
#include "executables/nr-uesoftmodem.h"
#include "PHY/phy_extern_nr_ue.h"
#include "PHY/INIT/nr_phy_init.h"
#include "NR_MAC_UE/mac_proto.h"
#include "RRC/NR_UE/rrc_proto.h"
#include "RRC/NR_UE/L2_interface_ue.h"
#include "SCHED_NR_UE/phy_frame_config_nr.h"
#include "SCHED_NR_UE/defs.h"
#include "PHY/NR_UE_TRANSPORT/nr_transport_proto_ue.h"
#include "executables/softmodem-common.h"
#include "PHY/NR_REFSIG/refsig_defs_ue.h"
#include "radio/COMMON/common_lib.h"
#include "LAYER2/nr_pdcp/nr_pdcp_oai_api.h"
#include "LAYER2/nr_rlc/nr_rlc_oai_api.h"
#include "RRC/NR/MESSAGES/asn1_msg.h"
#include "openair1/PHY/TOOLS/phy_scope_interface.h"
#include "PHY/MODULATION/nr_modulation.h"
#include "instrumentation.h"
#include "common/utils/threadPool/notified_fifo.h"
#include "position_interface.h"

/*
 *  NR SLOT PROCESSING SEQUENCE
 *
 *  Processing occurs with following steps for connected mode:
 *
 *  - Rx samples for a slot are received,
 *  - PDCCH processing (including DCI extraction for downlink and uplink),
 *  - PDSCH processing (including transport blocks decoding),
 *  - PUCCH/PUSCH (transmission of acknowledgements, CSI, ... or data).
 *
 *  Time between reception of the slot and related transmission depends on UE processing performance.
 *  It is defined by the value NR_UE_CAPABILITY_SLOT_RX_TO_TX.
 *
 *  In NR, network gives the duration between Rx slot and Tx slot in the DCI:
 *  - for reception of a PDSCH and its associated acknowledgment slot (with a PUCCH or a PUSCH),
 *  - for reception of an uplink grant and its associated PUSCH slot.
 *
 *  So duration between reception and it associated transmission depends on its transmission slot given in the DCI.
 *  NR_UE_CAPABILITY_SLOT_RX_TO_TX means the minimum duration but higher duration can be given by the network because UE can support it.
 *
 *                                                                                                    Slot k
 *                                                                                  -------+------------+--------
 *                Frame                                                                    | Tx samples |
 *                Subframe                                                                 |   buffer   |
 *                Slot n                                                            -------+------------+--------
 *       ------ +------------+--------                                                     |
 *              | Rx samples |                                                             |
 *              |   buffer   |                                                             |
 *       -------+------------+--------                                                     |
 *                           |                                                             |
 *                           V                                                             |
 *                           +------------+                                                |
 *                           |   PDCCH    |                                                |
 *                           | processing |                                                |
 *                           +------------+                                                |
 *                           |            |                                                |
 *                           |            v                                                |
 *                           |            +------------+                                   |
 *                           |            |   PDSCH    |                                   |
 *                           |            | processing | decoding result                   |
 *                           |            +------------+    -> ACK/NACK of PDSCH           |
 *                           |                         |                                   |
 *                           |                         v                                   |
 *                           |                         +-------------+------------+        |
 *                           |                         | PUCCH/PUSCH | Tx samples |        |
 *                           |                         |  processing | transfer   |        |
 *                           |                         +-------------+------------+        |
 *                           |                                                             |
 *                           |/___________________________________________________________\|
 *                            \  duration between reception and associated transmission   /
 *
 * Remark: processing is done slot by slot, it can be distribute on different threads which are executed in parallel.
 * This is an architecture optimization in order to cope with real time constraints.
 * By example, for LTE, subframe processing is spread over 4 different threads.
 *
 */

static void *NRUE_phy_stub_standalone_pnf_task(void *arg);

static void start_process_slot_tx(void* arg) {
  notifiedFIFO_elt_t *newTx = arg;
  nr_rxtx_thread_data_t *curMsgTx = NotifiedFifoData(newTx);
  pushNotifiedFIFO(&curMsgTx->UE->ul_actor.fifo, newTx);
}

static size_t dump_L1_UE_meas_stats(PHY_VARS_NR_UE *ue, char *output, size_t max_len)
{
  const char *begin = output;
  const char *end = output + max_len;
  for (int i = 0; i < MAX_CPU_STAT_TYPE; i++) {
    output += print_meas_log(&ue->phy_cpu_stats.cpu_time_stats[i],
                             ue->phy_cpu_stats.cpu_time_stats[i].meas_name,
                             NULL,
                             NULL,
                             output,
                             end - output);
  }
  return output - begin;
}

static void *nrL1_UE_stats_thread(void *param)
{
  PHY_VARS_NR_UE *ue = (PHY_VARS_NR_UE *) param;
  const int max_len = 16384;
  char output[max_len];
  char filename[30];
  snprintf(filename, 29, "nrL1_UE_stats-%d.log", ue->Mod_id);
  filename[29] = 0;
  FILE *fd = fopen(filename, "w");
  AssertFatal(fd != NULL, "Cannot open %s\n", filename);

  while (!oai_exit) {
    sleep(1);
    const int len = dump_L1_UE_meas_stats(ue, output, max_len);
    AssertFatal(len < max_len, "exceeded length\n");
    fwrite(output, len + 1, 1, fd); // + 1 for terminating NULL byte
    fflush(fd);
    fseek(fd, 0, SEEK_SET);
  }
  fclose(fd);

  return NULL;
}

void init_nr_ue_vars(PHY_VARS_NR_UE *ue, uint8_t UE_id)
{
  int nb_connected_gNB = 1;

  ue->Mod_id      = UE_id;
  ue->if_inst     = nr_ue_if_module_init(UE_id);
  ue->dci_thres   = 0;
  ue->target_Nid_cell = -1;

  ue->ntn_config_message = CALLOC(1, sizeof(*ue->ntn_config_message));
  ue->ntn_config_message->update = false;

  // initialize all signal buffers
  init_nr_ue_signal(ue, nb_connected_gNB);

  // intialize transport
  init_nr_ue_transport(ue);

  // init N_TA offset
  init_N_TA_offset(ue);
}

void init_nrUE_standalone_thread(int ue_idx)
{
  int standalone_tx_port = 3611 + ue_idx * 2;
  int standalone_rx_port = 3612 + ue_idx * 2;
  nrue_init_standalone_socket(standalone_tx_port, standalone_rx_port);

  NR_UE_MAC_INST_t *mac = get_mac_inst(0);
  pthread_mutex_init(&mac->mutex_dl_info, NULL);

  pthread_t thread;
  if (pthread_create(&thread, NULL, nrue_standalone_pnf_task, NULL) != 0) {
    LOG_E(NR_MAC, "pthread_create failed for calling nrue_standalone_pnf_task");
  }
  pthread_setname_np(thread, "oai:nrue-stand");
  pthread_t phy_thread;
  if (pthread_create(&phy_thread, NULL, NRUE_phy_stub_standalone_pnf_task, NULL) != 0) {
    LOG_E(NR_MAC, "pthread_create failed for calling NRUE_phy_stub_standalone_pnf_task");
  }
  pthread_setname_np(phy_thread, "oai:nrue-stand-phy");
}

static void process_queued_nr_nfapi_msgs(NR_UE_MAC_INST_t *mac, int sfn, int slot)
{
  struct sfn_slot_s sfn_slot = {.sfn = sfn, .slot = slot};
  nfapi_nr_rach_indication_t *rach_ind = unqueue_matching(&nr_rach_ind_queue, MAX_QUEUE_SIZE, sfn_slot_matcher, &sfn_slot);
  nfapi_nr_dl_tti_request_t *dl_tti_request = get_queue(&nr_dl_tti_req_queue);
  nfapi_nr_ul_dci_request_t *ul_dci_request = get_queue(&nr_ul_dci_req_queue);

  for (int i = 0; i < NR_MAX_HARQ_PROCESSES; i++) {
    LOG_D(NR_MAC,
          "Try to get a ul_tti_req by matching CRC active sfn/slot %d.%d from queue with %lu items\n",
          sfn,
          slot,
          nr_ul_tti_req_queue.num_items);
    struct sfn_slot_s sfn_sf = {.sfn = mac->nr_ue_emul_l1.harq[i].active_ul_harq_sfn, .slot = mac->nr_ue_emul_l1.harq[i].active_ul_harq_slot };
    nfapi_nr_ul_tti_request_t *ul_tti_request_crc = unqueue_matching(&nr_ul_tti_req_queue, MAX_QUEUE_SIZE, sfn_slot_matcher, &sfn_sf);
    if (ul_tti_request_crc && ul_tti_request_crc->n_pdus > 0) {
      check_and_process_dci(NULL, NULL, NULL, ul_tti_request_crc);
      free_and_zero(ul_tti_request_crc);
    }
  }

  if (rach_ind && rach_ind->number_of_pdus > 0) {
      NR_UL_IND_t UL_INFO = {
        .rach_ind = *rach_ind,
      };
      send_nsa_standalone_msg(&UL_INFO, rach_ind->header.message_id);
      free_and_zero(rach_ind->pdu_list);
      free_and_zero(rach_ind);
  }
  if (dl_tti_request) {
    struct sfn_slot_s sfn_slot = {.sfn = dl_tti_request->SFN, .slot = dl_tti_request->Slot};
    nfapi_nr_tx_data_request_t *tx_data_request = unqueue_matching(&nr_tx_req_queue, MAX_QUEUE_SIZE, sfn_slot_matcher, &sfn_slot);
    if (!tx_data_request) {
      LOG_E(NR_MAC, "[%d.%d] No corresponding tx_data_request for given dl_tti_request sfn/slot\n",
            dl_tti_request->SFN, dl_tti_request->Slot);
      if (get_softmodem_params()->nsa)
        save_nr_measurement_info(dl_tti_request);
      free_and_zero(dl_tti_request);
    }
    else if (dl_tti_request->dl_tti_request_body.nPDUs > 0 && tx_data_request->Number_of_PDUs > 0) {
      if (get_softmodem_params()->nsa)
        save_nr_measurement_info(dl_tti_request);
      check_and_process_dci(dl_tti_request, tx_data_request, NULL, NULL);
      free_and_zero(dl_tti_request);
      free_and_zero(tx_data_request);
    }
    else {
      AssertFatal(false, "We dont have PDUs in either dl_tti %d or tx_req %d\n",
                  dl_tti_request->dl_tti_request_body.nPDUs, tx_data_request->Number_of_PDUs);
    }
  }
  if (ul_dci_request && ul_dci_request->numPdus > 0) {
    check_and_process_dci(NULL, NULL, ul_dci_request, NULL);
    free_and_zero(ul_dci_request);
  }
}

static void *NRUE_phy_stub_standalone_pnf_task(void *arg)
{
  LOG_I(MAC, "Clearing Queues\n");
  reset_queue(&nr_rach_ind_queue);
  reset_queue(&nr_rx_ind_queue);
  reset_queue(&nr_crc_ind_queue);
  reset_queue(&nr_uci_ind_queue);
  reset_queue(&nr_dl_tti_req_queue);
  reset_queue(&nr_tx_req_queue);
  reset_queue(&nr_ul_dci_req_queue);
  reset_queue(&nr_ul_tti_req_queue);

  int last_sfn_slot = -1;
  uint16_t sfn_slot = 0;

  module_id_t mod_id = 0;
  NR_UE_MAC_INST_t *mac = get_mac_inst(mod_id);
  for (int i = 0; i < NR_MAX_HARQ_PROCESSES; i++) {
      mac->nr_ue_emul_l1.harq[i].active = false;
      mac->nr_ue_emul_l1.harq[i].active_ul_harq_sfn = -1;
      mac->nr_ue_emul_l1.harq[i].active_ul_harq_slot = -1;
  }

  while (!oai_exit) {
    if (sem_wait(&sfn_slot_semaphore) != 0) {
      LOG_E(NR_MAC, "sem_wait() error\n");
      abort();
    }
    uint16_t *slot_ind = get_queue(&nr_sfn_slot_queue);
    nr_phy_channel_params_t *ch_info = get_queue(&nr_chan_param_queue);
    if (!slot_ind && !ch_info) {
      LOG_D(MAC, "get nr_sfn_slot_queue and nr_chan_param_queue == NULL!\n");
      continue;
    }
    if (slot_ind) {
      sfn_slot = *slot_ind;
      free_and_zero(slot_ind);
    }
    else if (ch_info) {
      sfn_slot = ch_info->sfn_slot;
      free_and_zero(ch_info);
    }

    int mu = 1; // NR-UE emul-L1 is hardcoded to 30kHZ, see check_and_process_dci()
    frame_t frame = NFAPI_SFNSLOTDEC2SFN(mu, sfn_slot);
    int slot = NFAPI_SFNSLOTDEC2SLOT(mu, sfn_slot);
    if (sfn_slot == last_sfn_slot) {
      LOG_D(NR_MAC, "repeated sfn_sf = %d.%d\n",
            frame, slot);
      continue;
    }
    last_sfn_slot = sfn_slot;

    LOG_D(NR_MAC, "The received sfn/slot [%d %d] from proxy\n",
          frame, slot);

    if (IS_SA_MODE(get_softmodem_params()) && mac->mib == NULL) {
      LOG_D(NR_MAC, "We haven't gotten MIB. Lets see if we received it\n");
      nr_ue_dl_indication(&mac->dl_info);
      process_queued_nr_nfapi_msgs(mac, frame, slot);
    }

    int CC_id = 0;
    uint8_t gNB_id = 0;
    int slots_per_frame = 20; //30 kHZ subcarrier spacing
    int slot_ahead = 2; // TODO: Make this dynamic
    nr_uplink_indication_t ul_info = {.cc_id = CC_id,
                                      .gNB_index = gNB_id,
                                      .module_id = mod_id,
                                      .slot = (slot + slot_ahead) % slots_per_frame,
                                      .frame = (slot + slot_ahead >= slots_per_frame) ? (frame + 1) % 1024 : frame};

    if (pthread_mutex_lock(&mac->mutex_dl_info)) abort();

    if (ch_info) {
      mac->nr_ue_emul_l1.pmi = ch_info->csi[0].pmi;
      mac->nr_ue_emul_l1.ri = ch_info->csi[0].ri;
      mac->nr_ue_emul_l1.cqi = ch_info->csi[0].cqi;
      free_and_zero(ch_info);
    }

    if (is_nr_DL_slot(mac->tdd_UL_DL_ConfigurationCommon, slot)) {
      memset(&mac->dl_info, 0, sizeof(mac->dl_info));
      mac->dl_info.cc_id = CC_id;
      mac->dl_info.gNB_index = gNB_id;
      mac->dl_info.module_id = mod_id;
      mac->dl_info.frame = frame;
      mac->dl_info.slot = slot;
      mac->dl_info.dci_ind = NULL;
      mac->dl_info.rx_ind = NULL;
      nr_ue_dl_indication(&mac->dl_info);
    }

    if (pthread_mutex_unlock(&mac->mutex_dl_info)) abort();

    if (is_nr_UL_slot(mac->tdd_UL_DL_ConfigurationCommon, ul_info.slot, mac->frame_type)) {
      LOG_D(NR_MAC, "Slot %d. calling nr_ue_ul_ind()\n", ul_info.slot);
      nr_ue_ul_scheduler(mac, &ul_info);
    }
    process_queued_nr_nfapi_msgs(mac, frame, slot);
  }
  return NULL;
}


/*!
 * It performs band scanning and synchonization.
 * \param arg is a pointer to a \ref PHY_VARS_NR_UE structure.
 */

typedef struct {
  PHY_VARS_NR_UE *UE;
  UE_nr_rxtx_proc_t proc;
  nr_gscn_info_t gscnInfo[MAX_GSCN_BAND];
  int numGscn;
  int rx_offset;
} syncData_t;

static int nr_ue_adjust_rx_gain(PHY_VARS_NR_UE *UE, openair0_config_t *cfg0, int gain_change)
{
  // Increase the RX gain by the value determined by adjust_rxgain
  cfg0->rx_gain[0] += gain_change;

  // Set new RX gain.
  int ret_gain = UE->rfdevice.trx_set_gains_func(&UE->rfdevice, cfg0);
  // APPLY RX gain again if crossed the MAX RX gain threshold
  if (ret_gain < 0) {
    gain_change += ret_gain;
    cfg0->rx_gain[0] += ret_gain;
    ret_gain = UE->rfdevice.trx_set_gains_func(&UE->rfdevice, cfg0);
  }

  int applied_rxgain = cfg0->rx_gain[0] - cfg0->rx_gain_offset[0];
  LOG_I(PHY, "Rxgain adjusted by %d dB, RX gain: %d dB \n", gain_change, applied_rxgain);

  return gain_change;
}

static void UE_synch(void *arg) {
  syncData_t *syncD = (syncData_t *)arg;
  PHY_VARS_NR_UE *UE = syncD->UE;
  UE->is_synchronized = 0;
  openair0_config_t *cfg0 = &openair0_cfg[UE->rf_map.card];

  if (UE->target_Nid_cell != -1) {
    LOG_W(NR_PHY, "Starting re-sync detection for target Nid_cell %i\n", UE->target_Nid_cell);
  } else {
    LOG_W(NR_PHY, "Starting sync detection\n");
  }

  LOG_I(PHY, "[UE thread Synch] Running Initial Synch \n");

  uint64_t dl_carrier, ul_carrier;
  NR_DL_FRAME_PARMS *fp = &UE->frame_parms;
  nr_initial_sync_t ret = {false, 0, 0};
  if (UE->sl_mode == 2) {
    fp = &UE->SL_UE_PHY_PARAMS.sl_frame_params;
    dl_carrier = fp->sl_CarrierFreq;
    ul_carrier = fp->sl_CarrierFreq;
    ret = sl_nr_slss_search(UE, &syncD->proc, SL_NR_SSB_REPETITION_IN_FRAMES);
  } else {
    nr_get_carrier_frequencies(UE, &dl_carrier, &ul_carrier);
    ret = nr_initial_sync(&syncD->proc, UE, 2, IS_SA_MODE(get_softmodem_params()), syncD->gscnInfo, syncD->numGscn);
  }

  if (ret.cell_detected) {
    syncD->rx_offset = ret.rx_offset;
    const int freq_offset = UE->common_vars.freq_offset; // frequency offset computed with pss in initial sync
    const int hw_slot_offset =
        ((ret.rx_offset << 1) / fp->samples_per_subframe * fp->slots_per_subframe)
        + round((float)((ret.rx_offset << 1) % fp->samples_per_subframe) / fp->samples_per_slot0);

    // rerun with new cell parameters and frequency-offset
    // todo: the freq_offset computed on DL shall be scaled before being applied to UL
    nr_rf_card_config_freq(cfg0, ul_carrier, dl_carrier, freq_offset);

    if (get_nrUE_params()->agc) {
      nr_ue_adjust_rx_gain(UE, cfg0, UE->adjust_rxgain);
    }

    LOG_I(PHY,
          "Got synch: hw_slot_offset %d, carrier off %d Hz, rxgain %f (DL %f Hz, UL %f Hz)\n",
          hw_slot_offset,
          freq_offset,
          cfg0->rx_gain[0] - cfg0->rx_gain_offset[0],
          cfg0->rx_freq[0],
          cfg0->tx_freq[0]);

    UE->rfdevice.trx_set_freq_func(&UE->rfdevice, cfg0);
    UE->is_synchronized = 1;
  } else {
    int gain_change = 0;
    if (get_nrUE_params()->agc)
      gain_change = nr_ue_adjust_rx_gain(UE, cfg0, INCREASE_IN_RXGAIN);
    if (gain_change)
      LOG_I(PHY, "synch retry: Rx gain increased \n");
    else
      LOG_E(PHY, "synch Failed: \n");
  }
}

static void RU_write(nr_rxtx_thread_data_t *rxtxD, bool sl_tx_action)
{
  PHY_VARS_NR_UE *UE = rxtxD->UE;
  const fapi_nr_config_request_t *cfg = &UE->nrUE_config;
  const UE_nr_rxtx_proc_t *proc = &rxtxD->proc;

  NR_DL_FRAME_PARMS *fp = &UE->frame_parms;
  if (UE->sl_mode == 2)
    fp = &UE->SL_UE_PHY_PARAMS.sl_frame_params;

  void *txp[NB_ANTENNAS_TX];
  int slot = proc->nr_slot_tx;
  for (int i = 0; i < fp->nb_antennas_tx; i++)
    txp[i] = (void *)&UE->common_vars.txData[i][fp->get_samples_slot_timestamp(slot, fp, 0)];

  radio_tx_burst_flag_t flags = TX_BURST_INVALID;

  if (UE->received_config_request) {
    if (openair0_cfg[0].duplex_mode == duplex_mode_FDD || get_softmodem_params()->continuous_tx) {
      flags = TX_BURST_MIDDLE;
    // In case of Sidelink, USRP write needed only in case transmission
    // needs to be done in this slot and not based on tdd ULDL configuration.
    } else if (UE->sl_mode == 2) {
      if (sl_tx_action)
        flags = TX_BURST_START_AND_END;
    } else {
      int slots_frame = fp->slots_per_frame;
      int curr_slot = nr_ue_slot_select(cfg, slot);
      if (curr_slot != NR_DOWNLINK_SLOT) {
        int next_slot = nr_ue_slot_select(cfg, (slot + 1) % slots_frame);
        int prev_slot = nr_ue_slot_select(cfg, (slot + slots_frame - 1) % slots_frame);
        if (prev_slot == NR_DOWNLINK_SLOT)
          flags = TX_BURST_START;
        else if (next_slot == NR_DOWNLINK_SLOT)
          flags = TX_BURST_END;
        else
          flags = TX_BURST_MIDDLE;
      }
    }
  }

  openair0_timestamp writeTimestamp = proc->timestamp_tx;
  int writeBlockSize = rxtxD->writeBlockSize;
  if ( writeBlockSize > fp->get_samples_per_slot(proc->nr_slot_tx, fp)) {
    // if writeBlockSize gets longer that slot size, fill with dummy
    const int dummyBlockSize = writeBlockSize - fp->get_samples_per_slot(proc->nr_slot_tx, fp);
    int tmp = openair0_write_reorder(&UE->rfdevice, writeTimestamp, txp, dummyBlockSize, fp->nb_antennas_tx, flags);
    AssertFatal(tmp == dummyBlockSize, "");

    writeTimestamp += dummyBlockSize;
    writeBlockSize -= dummyBlockSize;
  }

  int tmp = openair0_write_reorder(&UE->rfdevice, writeTimestamp, txp, writeBlockSize, fp->nb_antennas_tx, flags);
  AssertFatal(tmp == writeBlockSize, "");

  for (int i = 0; i < fp->nb_antennas_tx; i++)
    memset(txp[i], 0, writeBlockSize);
}

void processSlotTX(void *arg)
{
  TracyCZone(ctx, true);
  nr_rxtx_thread_data_t *rxtxD = arg;
  const UE_nr_rxtx_proc_t *proc = &rxtxD->proc;
  PHY_VARS_NR_UE *UE = rxtxD->UE;
  nr_phy_data_tx_t phy_data = {0};
  bool sl_tx_action = false;

  if (UE->if_inst)
    UE->if_inst->slot_indication(UE->Mod_id);

  if (proc->tx_slot_type == NR_UPLINK_SLOT || proc->tx_slot_type == NR_MIXED_SLOT) {
    if (UE->sl_mode == 2 && proc->tx_slot_type == NR_SIDELINK_SLOT) {
      // trigger L2 to run ue_sidelink_scheduler thru IF module
      if (UE->if_inst != NULL && UE->if_inst->sl_indication != NULL) {
        start_meas(&UE->ue_ul_indication_stats);
        nr_sidelink_indication_t sl_indication = {.module_id = UE->Mod_id,
                                                  .gNB_index = proc->gNB_id,
                                                  .cc_id = UE->CC_id,
                                                  .frame_tx = proc->frame_tx,
                                                  .slot_tx = proc->nr_slot_tx,
                                                  .frame_rx = proc->frame_rx,
                                                  .slot_rx = proc->nr_slot_rx,
                                                  .slot_type = SIDELINK_SLOT_TYPE_TX,
                                                  .phy_data = &phy_data};

        UE->if_inst->sl_indication(&sl_indication);
        stop_meas(&UE->ue_ul_indication_stats);
      }

      if (phy_data.sl_tx_action) {

        AssertFatal((phy_data.sl_tx_action >= SL_NR_CONFIG_TYPE_TX_PSBCH &&
                     phy_data.sl_tx_action < SL_NR_CONFIG_TYPE_TX_MAXIMUM), "Incorrect SL TX Action Scheduled\n");

        phy_procedures_nrUE_SL_TX(UE, proc, &phy_data);

        sl_tx_action = true;
      }

    } else {
      // trigger L2 to run ue_scheduler thru IF module
      // [TODO] mapping right after NR initial sync
      if (UE->if_inst != NULL && UE->if_inst->ul_indication != NULL) {
        start_meas(&UE->ue_ul_indication_stats);
        nr_uplink_indication_t ul_indication = {.module_id = UE->Mod_id,
                                                .gNB_index = proc->gNB_id,
                                                .cc_id = UE->CC_id,
                                                .frame = proc->frame_tx,
                                                .slot = proc->nr_slot_tx,
                                                .phy_data = &phy_data};

        UE->if_inst->ul_indication(&ul_indication);
        stop_meas(&UE->ue_ul_indication_stats);
      }

      phy_procedures_nrUE_TX(UE, proc, &phy_data);
    }
  }
  RU_write(rxtxD, sl_tx_action);
  TracyCZoneEnd(ctx);
}

static uint64_t get_carrier_frequency(const int N_RB, const int mu, const uint32_t pointA_freq_khz)
{
  const uint64_t bw = (NR_NB_SC_PER_RB * N_RB) * MU_SCS(mu);
  const uint64_t carrier_freq = (pointA_freq_khz + bw / 2) * 1000;
  return carrier_freq;
}

static int handle_sync_req_from_mac(PHY_VARS_NR_UE *UE)
{
  NR_DL_FRAME_PARMS *fp = &UE->frame_parms;
  // Start synchronization with a target gNB
  if (UE->synch_request.received_synch_request == 1) {
    // if upper layers signal BW scan we do as instructed by command line parameter
    // if upper layers disable BW scan we set it to false
    if (UE->synch_request.synch_req.ssb_bw_scan)
      UE->UE_scan_carrier = get_nrUE_params()->UE_scan_carrier;
    else
      UE->UE_scan_carrier = false;
    UE->target_Nid_cell = UE->synch_request.synch_req.target_Nid_cell;

    const fapi_nr_ue_carrier_config_t *cfg = &UE->nrUE_config.carrier_config;
    uint64_t dl_CarrierFreq = get_carrier_frequency(fp->N_RB_DL, fp->numerology_index, cfg->dl_frequency);
    uint64_t ul_CarrierFreq = get_carrier_frequency(fp->N_RB_UL, fp->numerology_index, cfg->uplink_frequency);
    if (dl_CarrierFreq != fp->dl_CarrierFreq || ul_CarrierFreq != fp->ul_CarrierFreq) {
      fp->dl_CarrierFreq = dl_CarrierFreq;
      fp->ul_CarrierFreq = ul_CarrierFreq;
      nr_rf_card_config_freq(&openair0_cfg[UE->rf_map.card], ul_CarrierFreq, dl_CarrierFreq, 0);
      UE->rfdevice.trx_set_freq_func(&UE->rfdevice, &openair0_cfg[0]);
      init_symbol_rotation(fp);
    }

    /* Clearing UE harq while DL actors are active causes race condition.
        So we let the current execution to complete here.*/
    for (int i = 0; i < NUM_DL_ACTORS; i++) {
      flush_actor(UE->dl_actors + i);
    }
    flush_actor(&UE->ul_actor);

    clean_UE_harq(UE);
    UE->is_synchronized = 0;
    UE->synch_request.received_synch_request = 0;
    return 0;
  }
  return 1;
}

static int UE_dl_preprocessing(PHY_VARS_NR_UE *UE,
                               const UE_nr_rxtx_proc_t *proc,
                               int *tx_wait_for_dlsch,
                               nr_phy_data_t *phy_data,
                               bool *stats_printed)
{
  TracyCZone(ctx, true);
  int sampleShift = INT_MAX;
  NR_DL_FRAME_PARMS *fp = &UE->frame_parms;
  if (UE->sl_mode == 2)
    fp = &UE->SL_UE_PHY_PARAMS.sl_frame_params;

  // process what RRC thread sent to MAC
  MessageDef *msg = NULL;
  do {
    itti_poll_msg(TASK_MAC_UE, &msg);
    if (msg)
      process_msg_rcc_to_mac(msg);
  } while (msg);

  if (IS_SOFTMODEM_NOS1 || IS_SA_MODE(get_softmodem_params())) {
    /* send tick to RLC and PDCP every ms */
    if (proc->nr_slot_rx % fp->slots_per_subframe == 0) {
      void nr_rlc_tick(int frame, int subframe);
      void nr_pdcp_tick(int frame, int subframe);
      nr_rlc_tick(proc->frame_rx, proc->nr_slot_rx / fp->slots_per_subframe);
      nr_pdcp_tick(proc->frame_rx, proc->nr_slot_rx / fp->slots_per_subframe);
    }
  }

  bool dl_slot = false;
  if (proc->rx_slot_type == NR_DOWNLINK_SLOT || proc->rx_slot_type == NR_MIXED_SLOT) {
    dl_slot = true;
    if(UE->if_inst != NULL && UE->if_inst->dl_indication != NULL) {
      nr_downlink_indication_t dl_indication;
      nr_fill_dl_indication(&dl_indication, NULL, NULL, proc, UE, phy_data);
      UE->if_inst->dl_indication(&dl_indication);
    }

    sampleShift = pbch_pdcch_processing(UE, proc, phy_data);
    if (phy_data->dlsch[0].active && phy_data->dlsch[0].rnti_type == TYPE_C_RNTI_) {
      // indicate to tx thread to wait for DLSCH decoding
      const int ack_nack_slot = (proc->nr_slot_rx + phy_data->dlsch[0].dlsch_config.k1_feedback) % UE->frame_parms.slots_per_frame;
      tx_wait_for_dlsch[ack_nack_slot]++;
    }
  }
  if (fp->frame_type == FDD || !dl_slot) {
    // good time to print statistics, we don't have to spend time  to decode DCI
    if (proc->frame_rx % 128 == 0) {
      if (*stats_printed == false) {
        print_ue_mac_stats(UE->Mod_id, proc->frame_rx, proc->nr_slot_rx);
        *stats_printed = true;
      }
    } else {
      *stats_printed = false;
    }
  }

  if (UE->sl_mode == 2) {
    if (proc->rx_slot_type == NR_SIDELINK_SLOT) {
      phy_data->sl_rx_action = 0;
      if (UE->if_inst != NULL && UE->if_inst->sl_indication != NULL) {
        nr_sidelink_indication_t sl_indication;
        nr_fill_sl_indication(&sl_indication, NULL, NULL, proc, UE, phy_data);
        UE->if_inst->sl_indication(&sl_indication);
      }

      if (phy_data->sl_rx_action) {

        AssertFatal((phy_data->sl_rx_action >= SL_NR_CONFIG_TYPE_RX_PSBCH &&
                     phy_data->sl_rx_action < SL_NR_CONFIG_TYPE_RX_MAXIMUM), "Incorrect SL RX Action Scheduled\n");

        sampleShift = psbch_pscch_processing(UE, proc, phy_data);
      }
    }
  } else
    ue_ta_procedures(UE, proc->nr_slot_tx, proc->frame_tx);

  TracyCZoneEnd(ctx);
  return sampleShift;
}

void UE_dl_processing(void *arg) {
  TracyCZone(ctx, true);;
  nr_rxtx_thread_data_t *rxtxD = (nr_rxtx_thread_data_t *) arg;
  UE_nr_rxtx_proc_t *proc = &rxtxD->proc;
  PHY_VARS_NR_UE    *UE   = rxtxD->UE;
  nr_phy_data_t *phy_data = &rxtxD->phy_data;

  if (!UE->sl_mode)
    pdsch_processing(UE, proc, phy_data);

  TracyCZoneEnd(ctx);
}

void dummyWrite(PHY_VARS_NR_UE *UE,openair0_timestamp timestamp, int writeBlockSize) {
  NR_DL_FRAME_PARMS *fp = &UE->frame_parms;
  if (UE->sl_mode == 2)
    fp = &UE->SL_UE_PHY_PARAMS.sl_frame_params;

  void *dummy_tx[fp->nb_antennas_tx];
  // 2 because the function we call use pairs of int16_t implicitly as complex numbers
  int16_t dummy_tx_data[fp->nb_antennas_tx][2 * writeBlockSize];
  memset(dummy_tx_data, 0, sizeof(dummy_tx_data));
  for (int i = 0; i < fp->nb_antennas_tx; i++)
    dummy_tx[i]=dummy_tx_data[i];

  int tmp = UE->rfdevice.trx_write_func(&UE->rfdevice, timestamp, dummy_tx, writeBlockSize, fp->nb_antennas_tx, 4);
  AssertFatal(writeBlockSize == tmp, "");
}

void readFrame(PHY_VARS_NR_UE *UE,  openair0_timestamp *timestamp, bool toTrash) {
  NR_DL_FRAME_PARMS *fp = &UE->frame_parms;
  // two frames for initial sync
  int num_frames = 2;
  // In Sidelink worst case SL-SSB can be sent once in 16 frames
  if (UE->sl_mode == 2) {
    fp = &UE->SL_UE_PHY_PARAMS.sl_frame_params;
    num_frames = SL_NR_PSBCH_REPETITION_IN_FRAMES;
  }

  void *rxp[NB_ANTENNAS_RX];

  for (int x = 0; x < num_frames * NR_NUMBER_OF_SUBFRAMES_PER_FRAME; x++) { // two frames for initial sync
    for (int slot = 0; slot < fp->slots_per_subframe; slot++) {
      for (int i = 0; i < fp->nb_antennas_rx; i++) {
        if (toTrash)
          rxp[i] = malloc16(fp->get_samples_per_slot(slot, fp) * 4);
        else
          rxp[i] = ((void *)&UE->common_vars.rxdata[i][0])
                   + 4 * ((x * fp->samples_per_subframe) + fp->get_samples_slot_timestamp(slot, fp, 0));
      }

      int read_block_size = fp->get_samples_per_slot(slot, fp);
      int tmp = UE->rfdevice.trx_read_func(&UE->rfdevice, timestamp, rxp, read_block_size, fp->nb_antennas_rx);
      UEscopeCopy(UE, ueTimeDomainSamplesBeforeSync, rxp[0], sizeof(c16_t), 1, read_block_size, 0);
      AssertFatal(read_block_size == tmp, "");

      if (IS_SOFTMODEM_RFSIM)
        dummyWrite(UE, *timestamp, fp->get_samples_per_slot(slot, fp));
      if (toTrash)
        for (int i = 0; i < fp->nb_antennas_rx; i++)
          free(rxp[i]);
    }
  }
}

static void syncInFrame(PHY_VARS_NR_UE *UE, openair0_timestamp *timestamp, openair0_timestamp rx_offset)
{
  NR_DL_FRAME_PARMS *fp = &UE->frame_parms;
  if (UE->sl_mode == 2)
    fp = &UE->SL_UE_PHY_PARAMS.sl_frame_params;

  LOG_I(PHY, "Resynchronizing RX by %ld samples\n", rx_offset);

  if (IS_SOFTMODEM_IQPLAYER || IS_SOFTMODEM_IQRECORDER) {
    // Resynchonize by slot (will work with numerology 1 only)
    for (int size = rx_offset; size > 0; size -= fp->samples_per_subframe / 2) {
      int unitTransfer = size > fp->samples_per_subframe / 2 ? fp->samples_per_subframe / 2 : size;
      int tmp =
          UE->rfdevice.trx_read_func(&UE->rfdevice, timestamp, (void **)UE->common_vars.rxdata, unitTransfer, fp->nb_antennas_rx);
      DevAssert(unitTransfer == tmp);
    }
  } else {
    *timestamp += fp->get_samples_per_slot(1, fp);
    for (int size = rx_offset; size > 0; size -= fp->samples_per_subframe) {
      int unitTransfer = size > fp->samples_per_subframe ? fp->samples_per_subframe : size;
      // we write before read because gNB waits for UE to write and both executions halt
      // this happens here as the read size is samples_per_subframe which is very much larger than samp_per_slot
      if (IS_SOFTMODEM_RFSIM)
        dummyWrite(UE, *timestamp, unitTransfer);
      int res =
          UE->rfdevice.trx_read_func(&UE->rfdevice, timestamp, (void **)UE->common_vars.rxdata, unitTransfer, fp->nb_antennas_rx);
      DevAssert(unitTransfer == res);
      *timestamp += unitTransfer; // this does not affect the read but needed for RFSIM write
    }
  }
}

static inline int get_firstSymSamp(uint16_t slot, NR_DL_FRAME_PARMS *fp) {
  if (fp->numerology_index == 0)
    return fp->nb_prefix_samples0 + fp->ofdm_symbol_size;
  int num_samples = (slot%(fp->slots_per_subframe/2)) ? fp->nb_prefix_samples : fp->nb_prefix_samples0;
  num_samples += fp->ofdm_symbol_size;
  return num_samples;
}

static inline int get_readBlockSize(uint16_t slot, NR_DL_FRAME_PARMS *fp) {
  int rem_samples = fp->get_samples_per_slot(slot, fp) - get_firstSymSamp(slot, fp);
  int next_slot_first_symbol = 0;
  if (slot < (fp->slots_per_frame-1))
    next_slot_first_symbol = get_firstSymSamp(slot+1, fp);
  return rem_samples + next_slot_first_symbol;
}

void *UE_thread(void *arg)
{
  //this thread should be over the processing thread to keep in real time
  PHY_VARS_NR_UE *UE = (PHY_VARS_NR_UE *) arg;
  //  int tx_enabled = 0;
  void *rxp[NB_ANTENNAS_RX];
  enum stream_status_e stream_status = STREAM_STATUS_UNSYNC;
  fapi_nr_config_request_t *cfg = &UE->nrUE_config;
  int tmp = openair0_device_load(&(UE->rfdevice), &openair0_cfg[0]);
  AssertFatal(tmp == 0, "Could not load the device\n");
  NR_DL_FRAME_PARMS *fp = &UE->frame_parms;
  sl_nr_phy_config_request_t *sl_cfg = NULL;
  if (UE->sl_mode == 2) {
    fp = &UE->SL_UE_PHY_PARAMS.sl_frame_params;
    sl_cfg = &UE->SL_UE_PHY_PARAMS.sl_config;
  }

  UE->rfdevice.host_type = RAU_HOST;
  UE->is_synchronized = 0;
  int tmp2 = UE->rfdevice.trx_start_func(&UE->rfdevice);
  AssertFatal(tmp2 == 0, "Could not start the device\n");
  if (usrp_tx_thread == 1)
    UE->rfdevice.trx_write_init(&UE->rfdevice);

  notifiedFIFO_t nf;
  initNotifiedFIFO(&nf);

  notifiedFIFO_t freeBlocks;
  initNotifiedFIFO_nothreadSafe(&freeBlocks);

  int timing_advance = UE->timing_advance;
  NR_UE_MAC_INST_t *mac = get_mac_inst(UE->Mod_id);

  bool syncRunning = false;
  const int nb_slot_frame = fp->slots_per_frame;
  int absolute_slot = 0, decoded_frame_rx = MAX_FRAME_NUMBER - 1, trashed_frames = 0;
  int tx_wait_for_dlsch[NR_MAX_SLOTS_PER_FRAME];

  for(int i = 0; i < NUM_PROCESS_SLOT_TX_BARRIERS; i++) {
    dynamic_barrier_init(&UE->process_slot_tx_barriers[i]);
  }
  int shiftForNextFrame = 0;
  int intialSyncOffset = 0;
  openair0_timestamp sync_timestamp;
  bool stats_printed = false;

  if (get_softmodem_params()->sync_ref && UE->sl_mode == 2) {
    UE->is_synchronized = 1;
  } else {
    //warm up the RF board
    int64_t tmp;
    for (int i = 0; i < 50; i++)
      readFrame(UE, &tmp, true);
  }

  double ntn_ta_common = 0;
  int ntn_koffset = 0;

  int duration_rx_to_tx = NR_UE_CAPABILITY_SLOT_RX_TO_TX;
  int nr_slot_tx_offset = 0;
  bool update_ntn_system_information = false;

  while (!oai_exit) {
    nr_slot_tx_offset = 0;
    if (syncRunning) {
      notifiedFIFO_elt_t *res = pollNotifiedFIFO(&nf);

      if (res) {
        syncRunning = false;
        if (UE->is_synchronized) {
          UE->synch_request.received_synch_request = 0;
          if (UE->sl_mode == SL_MODE2_SUPPORTED)
            decoded_frame_rx = UE->SL_UE_PHY_PARAMS.sync_params.DFN;
          else {
            // We must wait the RRC layer decoded the MIB and sent us the frame number
            MessageDef *msg = NULL;
            itti_receive_msg(TASK_MAC_UE, &msg);
            if (msg)
              process_msg_rcc_to_mac(msg);
            else
              LOG_E(PHY, "It seems we arbort while trying to sync\n");
            decoded_frame_rx = mac->mib_frame;
          }
          LOG_A(PHY,
                "UE synchronized! decoded_frame_rx=%d UE->init_sync_frame=%d trashed_frames=%d\n",
                decoded_frame_rx,
                UE->init_sync_frame,
                trashed_frames);
          // shift the frame index with all the frames we trashed meanwhile we perform the synch search
          decoded_frame_rx = (decoded_frame_rx + UE->init_sync_frame + trashed_frames) % MAX_FRAME_NUMBER;
          syncData_t *syncMsg = (syncData_t *)NotifiedFifoData(res);
          intialSyncOffset = syncMsg->rx_offset;
        }
        delNotifiedFIFO_elt(res);
        stream_status = STREAM_STATUS_UNSYNC;
      } else {
        if (IS_SOFTMODEM_IQPLAYER || IS_SOFTMODEM_IQRECORDER) {
          /* For IQ recorder-player we force synchronization to happen in a fixed duration so that
             the replay runs in sync with recorded samples.
          */
          const unsigned int sync_in_frames = UE->rfdevice.openair0_cfg->recplay_conf->u_f_sync;
          while (trashed_frames != sync_in_frames) {
            readFrame(UE, &sync_timestamp, true);
            trashed_frames += 2;
          }
        } else {
          readFrame(UE, &sync_timestamp, true);
          trashed_frames += ((UE->sl_mode == 2) ? SL_NR_PSBCH_REPETITION_IN_FRAMES : 2);
        }
        continue;
      }
    }

    AssertFatal(!syncRunning, "At this point synchronization can't be running\n");

    if (!UE->is_synchronized) {
      readFrame(UE, &sync_timestamp, false);
      notifiedFIFO_elt_t *Msg = newNotifiedFIFO_elt(sizeof(syncData_t), 0, &nf, UE_synch);
      syncData_t *syncMsg = (syncData_t *)NotifiedFifoData(Msg);
      *syncMsg = (syncData_t){0};
      NR_DL_FRAME_PARMS *fp = &UE->frame_parms;
      if (UE->UE_scan_carrier) {
        // Get list of GSCN in this band for UE's bandwidth and center frequency.
        LOG_W(PHY, "UE set to scan all GSCN in current bandwidth\n");
        syncMsg->numGscn =
            get_scan_ssb_first_sc(fp->dl_CarrierFreq, fp->N_RB_DL, fp->nr_band, fp->numerology_index, syncMsg->gscnInfo);
      } else {
        LOG_W(PHY, "SSB position provided\n");
        syncMsg->gscnInfo[0] = (nr_gscn_info_t){.ssbFirstSC = fp->ssb_start_subcarrier};
        syncMsg->numGscn = 1;
      }
      syncMsg->UE = UE;
      memset(&syncMsg->proc, 0, sizeof(syncMsg->proc));
      pushNotifiedFIFO(&UE->sync_actor.fifo, Msg);
      trashed_frames = 0;
      syncRunning = true;
      continue;
    }

    if (stream_status == STREAM_STATUS_UNSYNC) {
      stream_status = STREAM_STATUS_SYNCING;
      syncInFrame(UE, &sync_timestamp, intialSyncOffset);
      openair0_write_reorder_clear_context(&UE->rfdevice);
      if (get_nrUE_params()->time_sync_I)
        // ntn_ta_commondrift is in µs/s, max_pos_acc * time_sync_I is in samples/frame
        UE->max_pos_acc = mac->ntn_ta.ntn_ta_commondrift * 1e-6 * fp->samples_per_frame / get_nrUE_params()->time_sync_I;
      else
        UE->max_pos_acc = 0;
      shiftForNextFrame = -(UE->init_sync_frame + trashed_frames + 2) * UE->max_pos_acc * get_nrUE_params()->time_sync_I; // compensate for the time drift that happened during initial sync
      LOG_D(PHY, "max_pos_acc = %d, shiftForNextFrame = %d\n", UE->max_pos_acc, shiftForNextFrame);
      // TODO: remove this autonomous TA and use up-to-date values of ta-Common, ta-CommonDrift and ta-CommonDriftVariant from received SIB19 instead
      if (get_nrUE_params()->autonomous_ta)
        UE->timing_advance -= 2 * shiftForNextFrame;
      // read in first symbol
      AssertFatal(fp->ofdm_symbol_size + fp->nb_prefix_samples0
                      == UE->rfdevice.trx_read_func(&UE->rfdevice,
                                                    &sync_timestamp,
                                                    (void **)UE->common_vars.rxdata,
                                                    fp->ofdm_symbol_size + fp->nb_prefix_samples0,
                                                    fp->nb_antennas_rx),
                  "");
      // we have the decoded frame index in the return of the synch process
      // and we shifted above to the first slot of next frame
      decoded_frame_rx = (decoded_frame_rx + 1) % MAX_FRAME_NUMBER;
      // we do ++ first in the regular processing, so it will be begin of frame;
      absolute_slot = decoded_frame_rx * nb_slot_frame - 1;
      if (UE->sl_mode == 2) {
        // Set to the slot where the SL-SSB was decoded
        absolute_slot += UE->SL_UE_PHY_PARAMS.sync_params.slot_offset;
      }
      // We have resynchronized, maybe after RF loss so we need to purge any existing context
      memset(tx_wait_for_dlsch, 0, sizeof(tx_wait_for_dlsch));
      for (int i = 0; i < NUM_PROCESS_SLOT_TX_BARRIERS; i++) {
        dynamic_barrier_reset(&UE->process_slot_tx_barriers[i]);
      }
      continue;
    }

    /* check if MAC has sent sync request */
    if (handle_sync_req_from_mac(UE) == 0)
      continue;

    // start of normal case, the UE is in sync
    absolute_slot++;
    TracyCFrameMark;

    if (update_ntn_system_information) {
      update_ntn_system_information = false;
      int ta_offset = UE->frame_parms.samples_per_subframe * (UE->ntn_config_message->ntn_config_params.ntn_total_time_advance_ms - ntn_ta_common);

      UE->timing_advance += ta_offset;
      ntn_ta_common = UE->ntn_config_message->ntn_config_params.ntn_total_time_advance_ms;
      ntn_koffset = UE->ntn_config_message->ntn_config_params.cell_specific_k_offset;
      timing_advance = ntn_koffset * (UE->frame_parms.samples_per_subframe >> mac->current_UL_BWP->scs);
    }

    if (UE->ntn_config_message->update) {
      UE->ntn_config_message->update = false;
      update_ntn_system_information = true;
      nr_slot_tx_offset = UE->ntn_config_message->ntn_config_params.cell_specific_k_offset;
    }

    int slot_nr = absolute_slot % nb_slot_frame;
    nr_rxtx_thread_data_t curMsg = {0};
    curMsg.UE=UE;
    // update thread index for received subframe
    curMsg.proc.nr_slot_rx  = slot_nr;
    curMsg.proc.nr_slot_tx  = (absolute_slot + duration_rx_to_tx) % nb_slot_frame;
    curMsg.proc.frame_rx    = (absolute_slot / nb_slot_frame) % MAX_FRAME_NUMBER;
    curMsg.proc.frame_tx    = ((absolute_slot + duration_rx_to_tx) / nb_slot_frame) % MAX_FRAME_NUMBER;
    if (UE->received_config_request) {
      if (UE->sl_mode) {
        curMsg.proc.rx_slot_type = sl_nr_ue_slot_select(sl_cfg, curMsg.proc.nr_slot_rx, TDD);
        curMsg.proc.tx_slot_type = sl_nr_ue_slot_select(sl_cfg, curMsg.proc.nr_slot_tx, TDD);
      } else {
        curMsg.proc.rx_slot_type = nr_ue_slot_select(cfg, curMsg.proc.nr_slot_rx);
        curMsg.proc.tx_slot_type = nr_ue_slot_select(cfg, curMsg.proc.nr_slot_tx);
      }
    }
    else {
      curMsg.proc.rx_slot_type = NR_DOWNLINK_SLOT;
      curMsg.proc.tx_slot_type = NR_DOWNLINK_SLOT;
    }

    int firstSymSamp = get_firstSymSamp(slot_nr, fp);
    for (int i = 0; i < fp->nb_antennas_rx; i++)
      rxp[i] = (void *)&UE->common_vars.rxdata[i][firstSymSamp + fp->get_samples_slot_timestamp(slot_nr, fp, 0)];

    int iq_shift_to_apply = 0;
    if (slot_nr == nb_slot_frame - 1) {
      // we shift of half of measured drift, at each beginning of frame for both rx and tx
      iq_shift_to_apply = shiftForNextFrame;
      // TODO: remove this autonomous TA and use up-to-date values of ta-Common, ta-CommonDrift and ta-CommonDriftVariant from received SIB19 instead
      if (get_nrUE_params()->autonomous_ta)
        UE->timing_advance -= 2 * shiftForNextFrame;
      shiftForNextFrame = -round(UE->max_pos_acc * get_nrUE_params()->time_sync_I);
    }

    const int readBlockSize = get_readBlockSize(slot_nr, fp) - iq_shift_to_apply;
    openair0_timestamp rx_timestamp;
    int tmp = UE->rfdevice.trx_read_func(&UE->rfdevice, &rx_timestamp, rxp, readBlockSize, fp->nb_antennas_rx);
    UEscopeCopy(UE, ueTimeDomainSamples, rxp[0], sizeof(c16_t), 1, readBlockSize, 0);
    AssertFatal(readBlockSize == tmp, "");

    if(slot_nr == (nb_slot_frame - 1)) {
      // read in first symbol of next frame and adjust for timing drift
      int first_symbols = fp->ofdm_symbol_size + fp->nb_prefix_samples0; // first symbol of every frames

      if (first_symbols > 0) {
        openair0_timestamp ignore_timestamp;
        int tmp = UE->rfdevice.trx_read_func(&UE->rfdevice,
                                             &ignore_timestamp,
                                             (void **)UE->common_vars.rxdata,
                                             first_symbols,
                                             fp->nb_antennas_rx);
        AssertFatal(first_symbols == tmp, "");

      } else
        LOG_E(PHY,"can't compensate: diff =%d\n", first_symbols);
    }

    // use previous timing_advance value to compute writeTimestamp
    const openair0_timestamp writeTimestamp = rx_timestamp + fp->get_samples_slot_timestamp(slot_nr, fp, duration_rx_to_tx)
                                              - firstSymSamp - UE->N_TA_offset - timing_advance;

    // but use current UE->timing_advance value to compute writeBlockSize
    int writeBlockSize = fp->get_samples_per_slot((slot_nr + duration_rx_to_tx) % nb_slot_frame, fp) - iq_shift_to_apply;
    if (UE->timing_advance != timing_advance) {
      writeBlockSize -= UE->timing_advance - timing_advance;
      timing_advance = UE->timing_advance;
    }

    if (curMsg.proc.nr_slot_tx == 0)
      nr_ue_rrc_timer_trigger(UE->Mod_id, curMsg.proc.frame_tx, curMsg.proc.gNB_id);

    // RX slot processing. We launch and forget.
    notifiedFIFO_elt_t *newRx = newNotifiedFIFO_elt(sizeof(nr_rxtx_thread_data_t), curMsg.proc.nr_slot_tx, NULL, UE_dl_processing);
    nr_rxtx_thread_data_t *curMsgRx = (nr_rxtx_thread_data_t *)NotifiedFifoData(newRx);
    *curMsgRx = (nr_rxtx_thread_data_t){.proc = curMsg.proc, .UE = UE};
    int ret = UE_dl_preprocessing(UE, &curMsgRx->proc, tx_wait_for_dlsch, &curMsgRx->phy_data, &stats_printed);
    if (ret != INT_MAX)
      shiftForNextFrame = ret;
    pushNotifiedFIFO(&UE->dl_actors[curMsg.proc.nr_slot_rx % NUM_DL_ACTORS].fifo, newRx);

    // Start TX slot processing here. It runs in parallel with RX slot processing
    // in current code, DURATION_RX_TO_TX constant is the limit to get UL data to encode from a RX slot
    notifiedFIFO_elt_t *newTx = newNotifiedFIFO_elt(sizeof(nr_rxtx_thread_data_t), 0, 0, processSlotTX);
    nr_rxtx_thread_data_t *curMsgTx = NotifiedFifoData(newTx);
    memset(curMsgTx, 0, sizeof(*curMsgTx));
    curMsgTx->proc = curMsg.proc;
    curMsgTx->writeBlockSize = writeBlockSize;
    curMsgTx->proc.timestamp_tx = writeTimestamp;
    curMsgTx->UE = UE;
    curMsgTx->proc.nr_slot_tx_offset = nr_slot_tx_offset;

    int slot = curMsgTx->proc.nr_slot_tx;
    int slot_and_frame = slot + curMsgTx->proc.frame_tx * UE->frame_parms.slots_per_frame;

    dynamic_barrier_update(&UE->process_slot_tx_barriers[slot_and_frame % NUM_PROCESS_SLOT_TX_BARRIERS],
                           tx_wait_for_dlsch[slot],
                           start_process_slot_tx,
                           newTx);
    stream_status = STREAM_STATUS_SYNCED;
    tx_wait_for_dlsch[slot] = 0;
    // apply new duration next run to avoid thread dead lock
    if (update_ntn_system_information) {
      duration_rx_to_tx = NR_UE_CAPABILITY_SLOT_RX_TO_TX + UE->ntn_config_message->ntn_config_params.cell_specific_k_offset;
    }
  }

  return NULL;
}

void init_NR_UE(int nb_inst, char *uecap_file, char *reconfig_file, char *rbconfig_file)
{
  NR_UE_RRC_INST_t *rrc_inst = nr_rrc_init_ue(uecap_file, nb_inst, get_nrUE_params()->nb_antennas_tx);
  NR_UE_MAC_INST_t *mac_inst = nr_l2_init_ue(nb_inst);
  AssertFatal(mac_inst, "Couldn't allocate MAC module\n");

  for (int i = 0; i < nb_inst; i++) {
    NR_UE_MAC_INST_t *mac = get_mac_inst(i);
    mac->if_module = nr_ue_if_module_init(i);
    AssertFatal(mac->if_module, "can not initialize IF module\n");
    if (!IS_SA_MODE(get_softmodem_params()) || !get_softmodem_params()->sl_mode) {
      init_nsa_message(&rrc_inst[i], reconfig_file, rbconfig_file);
      nr_rlc_activate_srb0(mac_inst[i].crnti, NULL, send_srb0_rrc);
    }
    //TODO: Move this call to RRC
    start_sidelink((&rrc_inst[i])->ue_id);
  }
}

void init_NR_UE_threads(PHY_VARS_NR_UE *UE) {
  pthread_t thread;
  char thread_name[16];
  sprintf(thread_name, "UEthread_%d", UE->Mod_id);
  threadCreate(&thread, UE_thread, (void *)UE, thread_name, -1, OAI_PRIORITY_RT_MAX);
  if (!IS_SOFTMODEM_NOSTATS) {
    pthread_t stat_pthread;
    sprintf(thread_name, "L1_UE_stats_%d", UE->Mod_id);
    threadCreate(&stat_pthread, nrL1_UE_stats_thread, UE, thread_name, -1, OAI_PRIORITY_RT_LOW);
  }
}
