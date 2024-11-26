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

static void wls_pnf_nr_handle_pnf_param_request(uint32_t msgSize, void *msg_buf)
{
  nfapi_nr_pnf_param_request_t req;
  nfapi_pnf_config_t *config = &(_this->_public);
  int unpack_result = config->unpack_func(msg_buf, msgSize, &req, sizeof(req), NULL);
  DevAssert(unpack_result >= 0);
  if (config->state == NFAPI_PNF_IDLE) {
    if (config->pnf_nr_param_req) {
      (config->pnf_nr_param_req)(config, &req);
    }
  } else {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: PNF not in IDLE state\n", __FUNCTION__);

    nfapi_nr_pnf_param_response_t resp;
    memset(&resp, 0, sizeof(resp));
    resp.header.message_id = NFAPI_NR_PHY_MSG_TYPE_PNF_PARAM_RESPONSE;
    resp.error_code = NFAPI_MSG_INVALID_STATE;
    nfapi_nr_pnf_pnf_param_resp(config, &resp);
  }
}

static void wls_pnf_nr_handle_pnf_config_request(uint32_t msgSize, void *msg_buf)
{
  nfapi_pnf_config_t *config = &(_this->_public);
  nfapi_nr_pnf_config_request_t req;
  int unpack_result = config->unpack_func(msg_buf, msgSize, &req, sizeof(req), NULL);
  DevAssert(unpack_result >= 0);
  // ensure correct state
  if (config->state != NFAPI_PNF_RUNNING) {
    // delete the phy records
    nfapi_pnf_phy_config_t *curr = config->phys;
    while (curr != 0) {
      nfapi_pnf_phy_config_t *to_delete = curr;
      curr = curr->next;
      free(to_delete);
    }
    config->phys = 0;

    // create the phy records
    if (req.pnf_phy_rf_config.tl.tag == NFAPI_PNF_PHY_RF_TAG) {
      int i = 0;
      for (i = 0; i < req.pnf_phy_rf_config.number_phy_rf_config_info; ++i) {
        nfapi_pnf_phy_config_t *phy = (nfapi_pnf_phy_config_t *)malloc(sizeof(nfapi_pnf_phy_config_t));
        memset(phy, 0, sizeof(nfapi_pnf_phy_config_t));

        phy->state = NFAPI_PNF_PHY_IDLE;
        phy->phy_id = req.pnf_phy_rf_config.phy_rf_config[i].phy_id;

        phy->next = config->phys;
        config->phys = phy;
      }
    }

    if (config->pnf_nr_config_req) {
      (config->pnf_nr_config_req)(config, &req);
    }
  } else {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: PNF not in correct state: %d\n", __FUNCTION__, config->state);

    nfapi_nr_pnf_config_response_t resp;
    memset(&resp, 0, sizeof(resp));
    resp.header.message_id = NFAPI_PNF_CONFIG_RESPONSE;
    resp.error_code = NFAPI_MSG_INVALID_STATE;
    nfapi_nr_pnf_pnf_config_resp(config, &resp);
  }
}

static void wls_pnf_nr_handle_pnf_start_request(uint32_t msg_size, void *msg_buf)
{
  nfapi_pnf_config_t *config = &(_this->_public);
  nfapi_nr_pnf_start_request_t req;
  int unpack_result = config->unpack_func(msg_buf, msg_size, &req, sizeof(req), NULL);
  DevAssert(unpack_result >= 0);
  if (config->state == NFAPI_PNF_CONFIGURED) {
    if (config->pnf_nr_start_req) {
      (config->pnf_nr_start_req)(config, &req);
    }
  } else {
    nfapi_nr_pnf_start_response_t resp;
    memset(&resp, 0, sizeof(resp));
    resp.header.message_id = NFAPI_PNF_START_RESPONSE;
    resp.error_code = NFAPI_MSG_INVALID_STATE;
    nfapi_nr_pnf_pnf_start_resp(config, &resp);
  }
}

static void wls_pnf_handle_pnf_stop_request(uint32_t msg_size, void *msg_buf)
{
  nfapi_pnf_config_t *config = &(_this->_public);
  nfapi_pnf_stop_request_t req;
  int unpack_result = config->unpack_func(msg_buf, msg_size, &req, sizeof(req), NULL);
  DevAssert(unpack_result >= 0);
  if (config->state == NFAPI_PNF_RUNNING) {
    if (config->pnf_stop_req) {
      (config->pnf_stop_req)(config, &req);
    }
  } else {
    nfapi_pnf_stop_response_t resp;
    memset(&resp, 0, sizeof(resp));
    resp.header.message_id = NFAPI_PNF_STOP_RESPONSE;
    resp.error_code = NFAPI_MSG_INVALID_STATE;
    nfapi_pnf_pnf_stop_resp(config, &resp);
  }
}

static void wls_pnf_nr_handle_param_request(uint32_t msg_size, void *msg_buf)
{
  nfapi_pnf_config_t *config = &(_this->_public);
  nfapi_nr_param_request_scf_t req;
  int unpack_result = cfg->unpack_func(msg_buf, msg_size, &req, sizeof(req), NULL);
  DevAssert(unpack_result >= 0);
  if (config->state == NFAPI_PNF_RUNNING) {
    nfapi_pnf_phy_config_t *phy = nfapi_pnf_phy_config_find(config, req.header.phy_id);
    if (phy) {
      if (phy->state == NFAPI_PNF_PHY_IDLE) {
        if (config->nr_param_req) {
          (config->nr_param_req)(config, phy, &req);
        }
      } else {
        nfapi_nr_param_response_scf_t resp;
        memset(&resp, 0, sizeof(resp));
        resp.header.message_id = NFAPI_NR_PHY_MSG_TYPE_PARAM_RESPONSE;
        resp.header.phy_id = req.header.phy_id;
        resp.error_code = NFAPI_MSG_INVALID_STATE;
        nfapi_nr_pnf_param_resp(config, &resp);
      }
    } else {
      nfapi_nr_param_response_scf_t resp;
      memset(&resp, 0, sizeof(resp));
      resp.header.message_id = NFAPI_NR_PHY_MSG_TYPE_PARAM_RESPONSE;
      resp.header.phy_id = req.header.phy_id;
      resp.error_code = NFAPI_MSG_INVALID_CONFIG;
      nfapi_nr_pnf_param_resp(config, &resp);
    }
  } else {
    nfapi_nr_param_response_scf_t resp;
    memset(&resp, 0, sizeof(resp));
    resp.header.message_id = NFAPI_NR_PHY_MSG_TYPE_PARAM_RESPONSE;
    resp.header.phy_id = req.header.phy_id;
    resp.error_code = NFAPI_MSG_INVALID_STATE;
    nfapi_nr_pnf_param_resp(config, &resp);
  }
}

static void wls_pnf_nr_handle_config_request(uint32_t msg_size, void *msg_buf)
{
  nfapi_nr_config_request_scf_t req = {0};
  nfapi_pnf_config_t *config = &(_this->_public);
  int unpack_result = cfg->unpack_func(msg_buf, msg_size, &req, sizeof(req), NULL);
  DevAssert(unpack_result >= 0);

  if (!isNFAPI) {
    // HACK for Radisys ODU, request them to pack this TLV
    req.carrier_config.dl_grid_size[1].value = 273;
    req.carrier_config.ul_grid_size[1].value = 273;

    req.nfapi_config.timing_window.tl.tag = NFAPI_NFAPI_TIMING_WINDOW_TAG;
    req.nfapi_config.timing_window.value = 30;
  }

  // TODO: Process and use the message
  if (config->state == NFAPI_PNF_RUNNING) {
    nfapi_pnf_phy_config_t *phy = nfapi_pnf_phy_config_find(config, req.header.phy_id);
    if (phy) {
      if (phy->state != NFAPI_PNF_PHY_RUNNING) {
        if (config->nr_config_req) {
          (config->nr_config_req)(config, phy, &req);
        }
      } else {
        nfapi_nr_config_response_scf_t resp;
        memset(&resp, 0, sizeof(resp));
        resp.header.message_id = NFAPI_CONFIG_RESPONSE;
        resp.header.phy_id = req.header.phy_id;
        resp.error_code = NFAPI_MSG_INVALID_STATE;
        printf("Try sending response back NFAPI_MSG_INVALID_STATE\n");
        nfapi_nr_pnf_config_resp(config, &resp);
      }
    } else {
      nfapi_nr_config_response_scf_t resp;
      memset(&resp, 0, sizeof(resp));
      resp.header.message_id = NFAPI_CONFIG_RESPONSE;
      resp.header.phy_id = req.header.phy_id;
      resp.error_code = NFAPI_MSG_INVALID_CONFIG;
      printf("Try sending response back NFAPI_MSG_INVALID_CONFIG\n");
      nfapi_nr_pnf_config_resp(config, &resp);
    }
  } else {
    nfapi_nr_config_response_scf_t resp;
    memset(&resp, 0, sizeof(resp));
    resp.header.message_id = NFAPI_CONFIG_RESPONSE;
    resp.header.phy_id = req.header.phy_id;
    resp.error_code = NFAPI_MSG_INVALID_STATE;
    printf("Try sending response back NFAPI_MSG_INVALID_STATE\n");
    nfapi_nr_pnf_config_resp(config, &resp);
  }
  // Free at the end
  free_config_request(&req);
}

static void wls_pnf_nr_handle_start_request(uint32_t msg_size, void *msg_buf)
{
  nfapi_nr_start_request_scf_t req = {0};
  nfapi_pnf_config_t *config = &(_this->_public);
  int unpack_result = cfg->unpack_func(msg_buf, msg_size, &req, sizeof(req), NULL);
  DevAssert(unpack_result >= 0);

  if (config->state == NFAPI_PNF_RUNNING) {
    nfapi_pnf_phy_config_t *phy = nfapi_pnf_phy_config_find(config, req.header.phy_id);
    if (phy) {
      if (phy->state != NFAPI_PNF_PHY_RUNNING) {
        if (config->nr_start_req) {
          (config->nr_start_req)(config, phy, &req);
        }
      } else {
        nfapi_nr_start_response_scf_t resp;
        memset(&resp, 0, sizeof(resp));
        resp.header.message_id = NFAPI_NR_PHY_MSG_TYPE_START_RESPONSE;
        resp.header.phy_id = req.header.phy_id;
        resp.error_code = NFAPI_NR_START_MSG_INVALID_STATE;
        nfapi_nr_pnf_start_resp(config, &resp);
      }
    } else {
      nfapi_nr_start_response_scf_t resp;
      memset(&resp, 0, sizeof(resp));
      resp.header.message_id = NFAPI_NR_PHY_MSG_TYPE_START_RESPONSE;
      resp.header.phy_id = req.header.phy_id;
      resp.error_code = NFAPI_NR_START_MSG_INVALID_STATE;
      nfapi_nr_pnf_start_resp(config, &resp);
    }
  } else {
    nfapi_nr_start_response_scf_t resp;
    memset(&resp, 0, sizeof(resp));
    resp.header.message_id = NFAPI_NR_PHY_MSG_TYPE_START_RESPONSE;
    resp.header.phy_id = req.header.phy_id;
    resp.error_code = NFAPI_NR_START_MSG_INVALID_STATE;
    nfapi_nr_pnf_start_resp(config, &resp);
  }
}

static void wls_pnf_nr_handle_stop_request(uint32_t msg_size, void *msg_buf)
{
  // ensure it's valid
  nfapi_nr_stop_request_scf_t req;
  nfapi_pnf_config_t *config = &(_this->_public);
  int unpack_result = cfg->unpack_func(msg_buf, msg_size, &req, sizeof(req), NULL);
  DevAssert(unpack_result >= 0);
  NFAPI_TRACE(NFAPI_TRACE_INFO, "STOP.request received\n");

  // unpack the message
  if (config->unpack_func(msg_buf, msg_size, &req, sizeof(req), &config->codec_config) >= 0) {
    if (config->state == NFAPI_PNF_RUNNING) {
      nfapi_pnf_phy_config_t *phy = nfapi_pnf_phy_config_find(config, req.header.phy_id);
      if (phy) {
        if (phy->state != NFAPI_PNF_PHY_RUNNING) {
          if (config->nr_stop_req) {
            (config->nr_stop_req)(config, phy, &req);
          }
        } else {
          nfapi_stop_response_t resp;
          memset(&resp, 0, sizeof(resp));
          resp.header.message_id = NFAPI_NR_PHY_MSG_TYPE_STOP_RESPONSE;
          resp.header.phy_id = req.header.phy_id;
          resp.error_code = NFAPI_MSG_INVALID_STATE;
          nfapi_pnf_stop_resp(config, &resp);
        }
      } else {
        nfapi_stop_response_t resp;
        memset(&resp, 0, sizeof(resp));
        resp.header.message_id = NFAPI_NR_PHY_MSG_TYPE_STOP_RESPONSE;
        resp.header.phy_id = req.header.phy_id;
        resp.error_code = NFAPI_MSG_INVALID_CONFIG;
        nfapi_pnf_stop_resp(config, &resp);
      }
    } else {
      nfapi_stop_response_t resp;
      memset(&resp, 0, sizeof(resp));
      resp.header.message_id = NFAPI_NR_PHY_MSG_TYPE_STOP_RESPONSE;
      resp.header.phy_id = req.header.phy_id;
      resp.error_code = NFAPI_MSG_INVALID_STATE;
      nfapi_pnf_stop_resp(config, &resp);
    }
  } else {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: Unpack message failed, ignoring\n", __FUNCTION__);
  }

  if (req.vendor_extension)
    _this->_public.codec_config.deallocate(req.vendor_extension);
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
      wls_pnf_nr_handle_pnf_param_request(msg_size + NFAPI_NR_P5_HEADER_LENGTH, msg_buf);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_PNF_CONFIG_REQUEST:
      wls_pnf_nr_handle_pnf_config_request(msg_size + NFAPI_NR_P5_HEADER_LENGTH, msg_buf);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_PNF_START_REQUEST:
      wls_pnf_nr_handle_pnf_start_request(msg_size + NFAPI_NR_P5_HEADER_LENGTH, msg_buf);
      break;

    case NFAPI_PNF_STOP_REQUEST:
      wls_pnf_handle_pnf_stop_request(msg_size + NFAPI_NR_P5_HEADER_LENGTH, msg_buf);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_PARAM_REQUEST:
      wls_pnf_nr_handle_param_request(msg_size + NFAPI_NR_P5_HEADER_LENGTH, msg_buf);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_CONFIG_REQUEST:
      wls_pnf_nr_handle_config_request(msg_size + (isNFAPI ? NFAPI_NR_P5_HEADER_LENGTH : 0), msg_buf);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_START_REQUEST:
      printf("\n NFAPI_NR_PHY_MSG_TYPE_START_REQUEST");
      wls_pnf_nr_handle_start_request(msg_size + (isNFAPI ? NFAPI_NR_P5_HEADER_LENGTH : 0), msg_buf);
      break;
    case NFAPI_NR_PHY_MSG_TYPE_STOP_REQUEST:
      wls_pnf_nr_handle_stop_request(msg_size, msg_buf);
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
