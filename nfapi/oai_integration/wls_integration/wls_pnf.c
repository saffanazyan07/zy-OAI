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

#include "wls_pnf.h"

#include <pnf_p7.h>
#include <sys/time.h>

#include "pnf.h"
#include "nr_nfapi_p7.h"
#include "nr_fapi_p5.h"
#include "nr_fapi_p7.h"

static bool isNFAPI = false;

nfapi_pnf_p7_config_t *wls_p7_config = NULL;

void wls_pnf_set_p7_config(nfapi_pnf_p7_config_t *p7_config)
{
  if (isNFAPI) {
    // The PNF has received nFAPI specific messages, set the pack/unpack funcs to be the nFAPI ones
    p7_config->unpack_func = &nfapi_nr_p7_message_unpack;
    p7_config->hdr_unpack_func = &nfapi_nr_p7_message_header_unpack;
    p7_config->pack_func = &nfapi_nr_p7_message_pack;
  }
  wls_p7_config = p7_config;
}

static WLS_MAC_CTX g_phy_wls;
uint8_t phy_dpdk_init(void);
uint8_t phy_wls_init(const char *dev_name, uint64_t nWlsMacMemSize, uint64_t nWlsPhyMemSize);
void phy_mac_recv();
void wls_mac_print_stats(void);

pnf_t *_this = NULL;

static PWLS_MAC_CTX wls_mac_get_ctx(void)
{
  return &g_phy_wls;
}


static nfapi_pnf_config_t *cfg;

void *wls_fapi_pnf_nr_start_thread(void *ptr)
{
  NFAPI_TRACE(NFAPI_TRACE_INFO, "[PNF] IN WLS PNF NFAPI start thread %s\n", __FUNCTION__);
  cfg = (nfapi_pnf_config_t *)ptr;
  wls_fapi_nr_pnf_start();
  return (void *)0;
}

int wls_fapi_nr_pnf_start()
{
  int64_t ret;
  // Verify that config is not null
  if (cfg == 0)
    return -1;

  NFAPI_TRACE(NFAPI_TRACE_INFO, "%s\n", __FUNCTION__);

  _this = (pnf_t *)(cfg);
  _this->terminate = 0;
  // Init PNF config
  nfapi_pnf_phy_config_t *phy = (nfapi_pnf_phy_config_t *)malloc(sizeof(nfapi_pnf_phy_config_t));
  memset(phy, 0, sizeof(nfapi_pnf_phy_config_t));

  phy->state = NFAPI_PNF_PHY_IDLE;
  phy->phy_id = 0;
  phy->next = (_this->_public).phys;
  (_this->_public).phys = phy;
  _this->_public.state = NFAPI_PNF_IDLE;

  // DPDK init
  ret = phy_dpdk_init();
  if (!ret) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "[PNF]  DPDK Init - Failed\n");
    return false;
  }
  NFAPI_TRACE(NFAPI_TRACE_INFO, "\n[PHY] DPDK Init - Done\n");

  // WLS init
  ret = phy_wls_init(WLS_DEV_NAME, WLS_MAC_MEMORY_SIZE, WLS_PHY_MEMORY_SIZE);
  if (!ret) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "[PNF]  WLS Init - Failed\n");
    return false;
  }
  NFAPI_TRACE(NFAPI_TRACE_INFO, "\n[PHY] WLS Init - Done\n");

  // Start Receiving loop from the VNF
  printf("Start Receiving loop from the VNF\n");
  phy_mac_recv();
  // Should never get here, to be removed, align with present implementation of PNF loop
  NFAPI_TRACE(NFAPI_TRACE_INFO, "\n[PHY] Exiting...\n");

  return true;
}

uint8_t phy_dpdk_init(void)
{
  unsigned long i;
  char *argv[] = {"OAI_PNF", "--proc-type=primary", "--file-prefix", WLS_DEV_NAME, "--iova-mode=pa", "--no-pci"};
  int argc = RTE_DIM(argv);
  /* initialize EAL first */
  NFAPI_TRACE(NFAPI_TRACE_INFO, "\n[PHY] Calling rte_eal_init: ");

  for (i = 0; i < RTE_DIM(argv); i++) {
    NFAPI_TRACE(NFAPI_TRACE_INFO, "%s ", argv[i]);
  }

  NFAPI_TRACE(NFAPI_TRACE_INFO, "\n");

  if (rte_eal_init(argc, argv) < 0)
    rte_panic("Cannot init EAL\n");

  return true;
}

uint8_t phy_wls_init(const char *dev_name, uint64_t nWlsMacMemSize, uint64_t nWlsPhyMemSize)
//uint8_t phy_wls_init(const char *dev_name,  uint64_t nBlockSize)
{
  PWLS_MAC_CTX pWls = wls_mac_get_ctx();
  pWls->nTotalAllocCnt = 0;
  pWls->nTotalFreeCnt = 0;
  pWls->nTotalUlBufAllocCnt = 0;
  pWls->nTotalUlBufFreeCnt = 0;
  pWls->nTotalDlBufAllocCnt = 0;
  pWls->nTotalDlBufFreeCnt = 0;

  pWls->hWls = WLS_Open(dev_name, WLS_SLAVE_CLIENT, &nWlsMacMemSize, &nWlsPhyMemSize);
  if (pWls->hWls) {
    /* allocate chuck of memory */
    if (WLS_Alloc(pWls->hWls, nWlsMacMemSize + nWlsPhyMemSize) != NULL) {
      printf("WLS Memory allocated successfully\n");
    } else {
      printf("Unable to alloc WLS Memory\n");
      return false;
    }
  } else {
    printf("can't open WLS instance");
    return false;
  }
  return true;
}

int wls_pnf_nr_pack_and_send_p5_message(pnf_t *pnf, nfapi_nr_p4_p5_message_header_t *msg, uint32_t msg_len)
{
  int packed_len = cfg->pack_func(msg,
                                  msg_len,
                                  pnf->tx_message_buffer,
                                  sizeof(pnf->tx_message_buffer),
                                  &pnf->_public.codec_config);

  if (packed_len < 0) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "nfapi_p5_message_pack failed (%d)\n", packed_len);
    return -1;
  }

  printf("fapi_nr_p5_message_pack succeeded having packed %d bytes\n", packed_len);
  printf("in msg, msg_len is %d\n", msg->message_length);
  PWLS_MAC_CTX pWls = wls_mac_get_ctx();

  int num_avail_blocks = WLS_NumBlocks(pWls->hWls);
  printf("num_avail_blocks is %d\n", num_avail_blocks);
  while (num_avail_blocks < 2) {
    num_avail_blocks = WLS_NumBlocks(pWls->hWls);
    printf("num_avail_blocks is %d\n", num_avail_blocks);
  }
  return wls_send_fapi_msg(pWls, msg->message_id, packed_len, pnf->tx_message_buffer);
}

int wls_pnf_nr_pack_and_send_p7_message(nfapi_nr_p7_message_header_t *msg)
{
  pnf_t *pnf = _this;

  if (!isFAPIMessageIDValid(msg->message_id)) {
    return -1;
  }

  // if working as FAPI and it's an nFAPI message, ignore
  if (!isNFAPI && (msg->message_id == NFAPI_NR_PHY_MSG_TYPE_UL_NODE_SYNC || msg->message_id == NFAPI_NR_PHY_MSG_TYPE_DL_NODE_SYNC
                   || msg->message_id == NFAPI_NR_PHY_MSG_TYPE_TIMING_INFO)) {
    return -1;
  }

  int packed_len = wls_p7_config->pack_func(msg, pnf->tx_message_buffer, sizeof(pnf->tx_message_buffer), NULL);

  if (packed_len < 0) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "nfapi_p7_message_pack failed (%d)\n", packed_len);
    return -1;
  }
  PWLS_MAC_CTX pWls = wls_mac_get_ctx();
  return wls_send_fapi_msg(pWls, msg->message_id, packed_len, pnf->tx_message_buffer);
}


extern void pnf_handle_dl_tti_request(void *pRecvMsg, int recvMsgLen, pnf_p7_t *pnf_p7, bool isNFAPI);
extern void pnf_handle_ul_tti_request(void *pRecvMsg, int recvMsgLen, pnf_p7_t *pnf_p7, bool isNFAPI);
extern void pnf_handle_ul_dci_request(void *pRecvMsg, int recvMsgLen, pnf_p7_t *pnf_p7, bool isNFAPI);
extern void pnf_handle_tx_data_request(void *pRecvMsg, int recvMsgLen, pnf_p7_t *pnf_p7, bool isNFAPI);
extern void pnf_nr_handle_dl_node_sync(void *pRecvMsg, int recvMsgLen, pnf_p7_t *pnf_p7, uint32_t rx_hr_time);

static void wls_pnf_nr_handle_p7_messages(uint32_t msg_size, void *rcv_msg, int msgId)
{
  if (rcv_msg == NULL || _this == NULL) {
    AssertFatal(rcv_msg != NULL && _this != NULL, "%s: NULL parameters\n", __FUNCTION__);
  }
  pnf_p7_t *pnf_p7 = (pnf_p7_t *)wls_p7_config;

  uint8_t *msg = rcv_msg;
  uint8_t *hdr = malloc(NFAPI_HEADER_LENGTH);
  memcpy(&hdr, &rcv_msg, NFAPI_HEADER_LENGTH);
  uint8_t *end = (uint8_t *)rcv_msg + msg_size + NFAPI_HEADER_LENGTH;
  // first, unpack the header
  fapi_msg_header fapi_msg;
  if (!pull8(&hdr, &fapi_msg.num_msg, end)) {
    AssertFatal(1 == 0, "FAPI num_msg unpack failed\n");
  }
  if (!pull8(&hdr, &fapi_msg.opaque_handle, end)) {
    AssertFatal(1 == 0, "FAPI opaque_handle unpack failed\n");
  }
  if (!pull16(&hdr, &fapi_msg.message_id, end)) {
    AssertFatal(1 == 0, "FAPI message_id unpack failed\n");
  }
  if (!pull32(&hdr, &fapi_msg.message_length, end)) {
    AssertFatal(1 == 0, "FAPI message_length unpack failed\n");
  }

  if (!check_nr_fapi_unpack_length(fapi_msg.message_id, fapi_msg.message_length + NFAPI_HEADER_LENGTH)) {
    printf("Message 0x%02x too short, ignoring\n", fapi_msg.message_id);
  } else {
    switch (msgId) {
      case NFAPI_NR_PHY_MSG_TYPE_DL_TTI_REQUEST: {
        pnf_handle_dl_tti_request(rcv_msg, msg_size + NFAPI_HEADER_LENGTH, pnf_p7, isNFAPI);
      }
      break;
      case NFAPI_NR_PHY_MSG_TYPE_UL_TTI_REQUEST: {
        pnf_handle_ul_tti_request(rcv_msg, msg_size + NFAPI_HEADER_LENGTH, pnf_p7, isNFAPI);
      }
      break;
      case NFAPI_NR_PHY_MSG_TYPE_UL_DCI_REQUEST: {
        pnf_handle_ul_dci_request(rcv_msg, msg_size + NFAPI_HEADER_LENGTH, pnf_p7, isNFAPI);
      }
      break;
      case NFAPI_NR_PHY_MSG_TYPE_TX_DATA_REQUEST: {
        pnf_handle_tx_data_request(rcv_msg, msg_size + NFAPI_HEADER_LENGTH, pnf_p7, isNFAPI);
      }
      break;
      case NFAPI_NR_PHY_MSG_TYPE_DL_NODE_SYNC: {
        struct timeval now;
        (void)gettimeofday(&now, NULL);
        uint32_t rx_hr_time = TIME2TIMEHR(now);
        pnf_nr_handle_dl_node_sync(msg, msg_size + NFAPI_HEADER_LENGTH, pnf_p7, rx_hr_time);
        break;
      }
      default:
        break;
    }
  }
}

static void procPhyMessages(uint32_t msg_size, void *msg_buf, uint16_t msg_id)
{
  // Should be able to be casted into already present fapi p5 header format, to fix, dependent on changes on L2 to follow
  // fapi_message_header_t in nr_fapi.h
  NFAPI_TRACE(NFAPI_TRACE_DEBUG, "[PHY] Received Msg ID 0x%02x\n", msg_id);
  if (msg_buf == NULL || _this == NULL) {
    AssertFatal(msg_buf != NULL && _this != NULL, "%s: NULL parameters\n", __FUNCTION__);
  }
  switch (msg_id) {
    case NFAPI_NR_PHY_MSG_TYPE_PNF_PARAM_REQUEST:
      // received an NFAPI message from VNF, mark as to use nFAPI functions from now on
      isNFAPI = true;
      // Set the P5 pack/unpack procedures to be the nFAPI ones
      cfg->unpack_func = &nfapi_nr_p5_message_unpack;
      cfg->hdr_unpack_func = &nfapi_nr_p5_message_header_unpack;
      cfg->pack_func = &nfapi_nr_p5_message_pack;
      printf("\n NFAPI_NR_PHY_MSG_TYPE_PNF_PARAM_REQUEST");
      pnf_nr_handle_pnf_param_request(_this,msg_buf, msg_size + NFAPI_NR_P5_HEADER_LENGTH);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_PNF_CONFIG_REQUEST:
      pnf_nr_handle_pnf_config_request(_this,msg_buf, msg_size + NFAPI_NR_P5_HEADER_LENGTH);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_PNF_START_REQUEST:
      pnf_nr_handle_pnf_start_request(_this,msg_buf, msg_size + NFAPI_NR_P5_HEADER_LENGTH);
      break;

    case NFAPI_PNF_STOP_REQUEST:
      pnf_handle_pnf_stop_request(_this,msg_buf, msg_size + NFAPI_NR_P5_HEADER_LENGTH);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_PARAM_REQUEST:
      pnf_nr_handle_param_request(_this,msg_buf, msg_size + NFAPI_NR_P5_HEADER_LENGTH);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_CONFIG_REQUEST:
      pnf_nr_handle_config_request(_this,msg_buf, msg_size + (isNFAPI ? NFAPI_NR_P5_HEADER_LENGTH : 0));
      break;

    case NFAPI_NR_PHY_MSG_TYPE_START_REQUEST:
      printf("\n NFAPI_NR_PHY_MSG_TYPE_START_REQUEST");
      pnf_nr_handle_start_request(_this,msg_buf, msg_size + (isNFAPI ? NFAPI_NR_P5_HEADER_LENGTH : 0));
      break;
    case NFAPI_NR_PHY_MSG_TYPE_STOP_REQUEST:
      pnf_nr_handle_stop_request(_this,msg_buf, msg_size + NFAPI_NR_P5_HEADER_LENGTH);
      break;
    case NFAPI_NR_PHY_MSG_TYPE_DL_TTI_REQUEST
    ...
    NFAPI_NR_PHY_MSG_TYPE_RACH_INDICATION: {
      wls_pnf_nr_handle_p7_messages(msg_size, msg_buf, msg_id);
      break;
    }
    default:
      printf("\n Unknown Msg ID 0x%02x\n", msg_id);
      break;
  }
}

void phy_mac_recv()
{
  /* Number of Memory blocks to get */
  uint32_t msgSize;
  uint16_t msgType;
  uint16_t flags;
  uint32_t i = 0;
  PWLS_MAC_CTX pWls = wls_mac_get_ctx();
  bool isRadisysFAPI = false;
  while (_this->terminate == 0) {
    //printf("Before wait\n");
    int numMsgToGet = WLS_Wait(pWls->hWls);
    if (numMsgToGet <= 0) {
      // printf("Wait returned 0\n");
      continue;
    }
    //printf("Got %d messages to process\n", numMsgToGet);
    while (numMsgToGet--) {
      p_fapi_api_queue_elem_t msg = NULL;
      const uint64_t msg_PA = WLS_Get(pWls->hWls, &msgSize, &msgType, &flags);
      if (msg_PA != 0) {
        //printf("WLS_Get Success\n");
        i++;
        void *msg_VA = WLS_PA2VA(pWls->hWls, msg_PA);
        msg = (p_fapi_api_queue_elem_t)msg_VA;

        //printf("msg->msg_type 0x%02x\n", msg->msg_type);
        //printf("msg->msg_len 0x%04x\n", msg->msg_len);
        if (msg->msg_type != FAPI_VENDOR_MSG_HEADER_IND) {
          uint8_t *msgt = (uint8_t *)(msg + 1);
          /*for (int x = 0; x < msg->msg_len; ++x) {
            printf("0x%02x ", msgt[x]);
          }*/

          uint16_t msg_id = msgt[3] | (msgt[2] << 8);
          //printf("\n ID 0x%02x\n", msg_id);
          uint32_t len = msgt[7] | (msgt[6] << 8) | (msgt[5] << 16) | (msgt[4] << 24);
          //printf("\n LEN 0x%04x\n", len);

          if (isRadisysFAPI) {
            if (_this->_public.state == NFAPI_PNF_IDLE) {
              //is FAPI , go straight to PNF running
              _this->_public.state = NFAPI_PNF_RUNNING; // not a nFAPI PNF, go immediately to PNF_RUNNING state
            }
          }
          procPhyMessages(isRadisysFAPI ? msg->msg_len: len, (void *)(msg + 1), msg_id);
        } else {
          uint8_t *msgt = (uint8_t *)(msg + 1);
          // Check the opaque handle value to determine if it's the OAI VNF, if yes, don't progress the FAPI PNF State machine
          if (msgt[1] != 0xff) {
            isRadisysFAPI = true;
          }
        }
      } else {
        NFAPI_TRACE(NFAPI_TRACE_ERROR, "\n[PHY] MAC2PHY WLS Get Error for msg %d\n", i);
        break;
      }
      if (flags & WLS_TF_FIN) {
        // Don't return, the messages responses will be handled in procPhyMessages
      }
    }
  }
}
