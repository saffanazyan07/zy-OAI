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
#include "wls_vnf.h"

#include <nr_fapi_p5.h>
#include <nr_fapi_p7.h>

static nfapi_vnf_config_t *config;
static WLS_MAC_CTX wls_mac_iface;
vnf_t *_vnf = NULL;
nfapi_vnf_p7_config_t *wls_vnf_p7_config = NULL;

void wls_vnf_set_p7_config(nfapi_vnf_p7_config_t *p7_config)
{
  wls_vnf_p7_config = p7_config;
}

static PWLS_MAC_CTX wls_mac_get_ctx(void)
{
  return &wls_mac_iface;
}

static uint8_t alloc_track[ALLOC_TRACK_SIZE];
//-------------------------------------------------------------------------------------------
/** @ingroup group_testmac
 *
 *  @param[in]   pMemArray Pointer to WLS Memory management structure
 *  @param[in]   pMemArrayMemory Pointer to flat buffer that was allocated
 *  @param[in]   totalSize Total Size of flat buffer allocated
 *  @param[in]   nBlockSize Size of each block that needs to be partitioned by memory manager
 *
 *  @return  0 if SUCCESS
 *
 *  @description
 *  This function creates memory blocks from a flat buffer which will be used for communciation
 *  between MAC and PHY
 *
 **/
//-------------------------------------------------------------------------------------------
static bool wls_mac_create_mem_array(PWLS_MAC_MEM_SRUCT pMemArray, void *pMemArrayMemory, uint32_t totalSize, uint32_t nBlockSize)
{
  const uint32_t numBlocks = totalSize / nBlockSize;

  printf("wls_mac_create_mem_array: pMemArray[%p] pMemArrayMemory[%p] totalSize[%d] nBlockSize[%d] numBlocks[%d]\n",
         pMemArray,
         pMemArrayMemory,
         totalSize,
         nBlockSize,
         numBlocks);
  AssertFatal(nBlockSize >= sizeof(void *), "WLS nBlockSize too small");
  AssertFatal(totalSize > sizeof(void *), "WLS Can't allocate less than 1 block");
  pMemArray->ppFreeBlock = (void **)pMemArrayMemory;
  pMemArray->pStorage = pMemArrayMemory;
  pMemArray->pEndOfStorage = ((unsigned long *)pMemArrayMemory) + numBlocks * nBlockSize / sizeof(unsigned long);
  pMemArray->nBlockSize = nBlockSize;
  pMemArray->nBlockCount = numBlocks;

  // Initialize single-linked list of free blocks;
  void **ptr = (void **)pMemArrayMemory;
  for (uint32_t i = 0; i < pMemArray->nBlockCount; i++) {
    if (i == pMemArray->nBlockCount - 1) {
      *ptr = NULL; // End of list
    } else {
      // Points to the next block
      *ptr = (void **)(((uint8_t *)ptr) + nBlockSize);
      ptr += nBlockSize / sizeof(unsigned long);
    }
  }

  memset(alloc_track, 0, sizeof(uint8_t) * ALLOC_TRACK_SIZE);

  return true;
}

void mac_dpdk_init()
{
  char *my_argv[] = {"OAI_VNF", "-c3", "--proc-type=auto", "--file-prefix", WLS_DEV_NAME, "--iova-mode=pa"};
  printf("\nCalling rte_eal_init: ");
  for (unsigned long i = 0; i < RTE_DIM(my_argv); i++) {
    printf("%s ", my_argv[i]);
  }
  printf("\n");

  if (rte_eal_init(RTE_DIM(my_argv), my_argv) < 0)
    rte_panic("\nCannot init EAL\n");
}

static uint32_t wls_mac_alloc_mem_array(PWLS_MAC_MEM_SRUCT pMemArray, void **ppBlock)
{
  int idx;

  if (pMemArray->ppFreeBlock == NULL) {
    printf("wls_mac_alloc_mem_array pMemArray->ppFreeBlock = NULL\n");
    return 1;
  }

  // FIXME: Remove after debugging
  if (((void *)pMemArray->ppFreeBlock < pMemArray->pStorage) || ((void *)pMemArray->ppFreeBlock >= pMemArray->pEndOfStorage)) {
    printf("wls_mac_alloc_mem_array ERROR: Corrupted MemArray;Arr=%p,Stor=%p,Free=%p\n",
           pMemArray,
           pMemArray->pStorage,
           pMemArray->ppFreeBlock);
    return 1;
  }

  pMemArray->ppFreeBlock = (void **)((unsigned long)pMemArray->ppFreeBlock & 0xFFFFFFFFFFFFFFF0);
  *pMemArray->ppFreeBlock = (void **)((unsigned long)*pMemArray->ppFreeBlock & 0xFFFFFFFFFFFFFFF0);

  if ((*pMemArray->ppFreeBlock != NULL)
      && (((*pMemArray->ppFreeBlock) < pMemArray->pStorage) || ((*pMemArray->ppFreeBlock) >= pMemArray->pEndOfStorage))) {
    fprintf(stderr,
            "ERROR: Corrupted MemArray;Arr=%p,Stor=%p,Free=%p,Curr=%p\n",
            pMemArray,
            pMemArray->pStorage,
            pMemArray->ppFreeBlock,
            *pMemArray->ppFreeBlock);
    return 1;
  }

  *ppBlock = (void *)pMemArray->ppFreeBlock;
  pMemArray->ppFreeBlock = (void **)(*pMemArray->ppFreeBlock);

  idx = (((uint64_t)*ppBlock - (uint64_t)pMemArray->pStorage)) / pMemArray->nBlockSize;
  if (alloc_track[idx]) {
    printf("wls_mac_alloc_mem_array Double alloc Arr=%p,Stor=%p,Free=%p,Curr=%p\n",
           pMemArray,
           pMemArray->pStorage,
           pMemArray->ppFreeBlock,
           *pMemArray->ppFreeBlock);
  } else {
    alloc_track[idx] = 1;
  }

  return 0;
}

void wls_mac_print_stats(void)
{
  PWLS_MAC_CTX pWls = wls_mac_get_ctx();

  printf("wls_mac_free_list_all:\n");
  printf("        nTotalBlocks[%d] nAllocBlocks[%d] nFreeBlocks[%d]\n",
         pWls->nTotalBlocks,
         pWls->nAllocBlocks,
         (pWls->nTotalBlocks - pWls->nAllocBlocks));
  printf("        nTotalAllocCnt[%d] nTotalFreeCnt[%d] Diff[%d]\n",
         pWls->nTotalAllocCnt,
         pWls->nTotalFreeCnt,
         (pWls->nTotalAllocCnt - pWls->nTotalFreeCnt));
  printf("        nDlBufAllocCnt[%d] nDlBufFreeCnt[%d] Diff[%d]\n",
         pWls->nTotalDlBufAllocCnt,
         pWls->nTotalDlBufFreeCnt,
         (pWls->nTotalDlBufAllocCnt - pWls->nTotalDlBufFreeCnt));
  printf("        nUlBufAllocCnt[%d] nUlBufFreeCnt[%d] Diff[%d]\n\n",
         pWls->nTotalUlBufAllocCnt,
         pWls->nTotalUlBufFreeCnt,
         (pWls->nTotalUlBufAllocCnt - pWls->nTotalUlBufFreeCnt));
}

static void *wls_mac_alloc_buffer(PWLS_MAC_CTX pWls, uint32_t size, uint32_t loc)
{
  void *pBlock = NULL;
  pthread_mutex_lock((pthread_mutex_t *)&pWls->lock_alloc);
  if (wls_mac_alloc_mem_array(&pWls->sWlsStruct, &pBlock) != 0) {
    printf("wls_mac_alloc_buffer alloc error size[%d] loc[%d]\n", size, loc);
    wls_mac_print_stats();
    return NULL;
  }
  pWls->nAllocBlocks++;
  pWls->nTotalAllocCnt++;
  if (loc < MAX_DL_BUF_LOCATIONS)
    pWls->nTotalDlBufAllocCnt++;
  else if (loc < MAX_UL_BUF_LOCATIONS)
    pWls->nTotalUlBufAllocCnt++;

  pthread_mutex_unlock((pthread_mutex_t *)&pWls->lock_alloc);

  return pBlock;
}

static bool mac_wls_init()
{
  uint64_t nWlsMacMemorySize = 0;
  uint64_t nWlsPhyMemorySize = 0;
  PWLS_MAC_CTX pWls = wls_mac_get_ctx();

  pthread_mutex_init((pthread_mutex_t *)&pWls->lock, NULL);
  pthread_mutex_init((pthread_mutex_t *)&pWls->lock_alloc, NULL);

  pWls->nTotalAllocCnt = 0;
  pWls->nTotalFreeCnt = 0;
  pWls->nTotalUlBufAllocCnt = 0;
  pWls->nTotalUlBufFreeCnt = 0;
  pWls->nTotalDlBufAllocCnt = 0;
  pWls->nTotalDlBufFreeCnt = 0;

  /* Start by opening the WLS instance with WLS_MASTER_CLIENT */
  if ((pWls->hWls = WLS_Open(WLS_DEV_NAME, WLS_MASTER_CLIENT, &nWlsMacMemorySize, &nWlsPhyMemorySize)) == NULL) {
    printf("\nCould not open WLS Interface \n");
    return false;
  }

  /* WLS_Open was successful, we can allocate memory */
  if ((pWls->pWlsMemBase = WLS_Alloc(pWls->hWls, nWlsMacMemorySize + nWlsPhyMemorySize)) == NULL) {
    printf("\nCould not allocate WLS Memory \n");
    return false;
  }
  /* Set the total memory that MAC has access to */
  pWls->nTotalMemorySize = nWlsMacMemorySize;

  /* Partition the WLS memory */
  memset(pWls->pWlsMemBase, 0xCC, pWls->nTotalMemorySize);
  pWls->pPartitionMemBase = pWls->pWlsMemBase;
  pWls->nPartitionMemSize = pWls->nTotalMemorySize / 2;
  pWls->nTotalBlocks = pWls->nTotalMemorySize / MSG_MAXSIZE;
  /* Create the memory array */
  if (!wls_mac_create_mem_array(&pWls->sWlsStruct, pWls->pPartitionMemBase, pWls->nPartitionMemSize, MSG_MAXSIZE)) {
    printf("\nCould not create WLS Memory Array \n");
    return false;
  }

  /* Enqueue all blocks */
  int nBlocks = WLS_EnqueueBlock(pWls->hWls, WLS_VA2PA(pWls->hWls, wls_mac_alloc_buffer(pWls, 0, MIN_UL_BUF_LOCATIONS + 2)));
  /* allocate blocks for UL transmition */
  while (WLS_EnqueueBlock(pWls->hWls, WLS_VA2PA(pWls->hWls, wls_mac_alloc_buffer(pWls, 0, MIN_UL_BUF_LOCATIONS + 3)))) {
    nBlocks++;
  }

  printf("\nAllocated %d Blocks \n", nBlocks);
  return true;
}

static int vnf_wls_init()
{
  mac_dpdk_init();

  if (!mac_wls_init()) {
    return 0;
  }

  return 1;
}

void *wls_fapi_vnf_nr_start_thread(void *ptr)
{
  NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] IN WLS PNF NFAPI start thread %s\n", __FUNCTION__);
  //pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp);
  wls_fapi_nr_vnf_start((nfapi_vnf_config_t *)ptr);
  return (void *)0;
}

static void wls_vnf_nr_handle_p7_messages(uint32_t msgSize, void *msg_buf, int msgId)
{
  uint8_t *msg = msg_buf;

  if (!check_nr_fapi_unpack_length(msgId, msgSize + NFAPI_NR_P7_HEADER_LENGTH)) {
    printf("Message 0x%02x too short, ignoring\n", msgId);
  } else {
    switch (msgId) {
      case NFAPI_NR_PHY_MSG_TYPE_SLOT_INDICATION: {
        nfapi_nr_slot_indication_scf_t unpacked_msg = {.header.message_id = msgId,
                                                       .header.message_length = msgSize};
        if (wls_vnf_p7_config->unpack_func(msg,
                                       msgSize + NFAPI_NR_P7_HEADER_LENGTH,
                                       &unpacked_msg,
                                       msgSize + NFAPI_NR_P7_HEADER_LENGTH,
                                       0) != 0) {
          printf("Failure unpacking, or dummy message, ignoring\n");
          break;
        }
        DevAssert(wls_vnf_p7_config->nr_slot_indication != NULL);
        (wls_vnf_p7_config->nr_slot_indication)(&unpacked_msg);
        free_slot_indication(&unpacked_msg);
      }
      break;
      case NFAPI_NR_PHY_MSG_TYPE_RX_DATA_INDICATION: {
        nfapi_nr_rx_data_indication_t unpacked_msg = {.header.message_id = msgId,
                                                      .header.message_length = msgSize};
        if (wls_vnf_p7_config->unpack_func(msg,
                                       msgSize + NFAPI_NR_P7_HEADER_LENGTH,
                                       &unpacked_msg,
                                       msgSize + NFAPI_NR_P7_HEADER_LENGTH,
                                       0) != 0) {
          printf("Failure unpacking, or dummy message, ignoring\n");
          break;
        }
        DevAssert(wls_vnf_p7_config->nr_rx_data_indication != NULL);
        (wls_vnf_p7_config->nr_rx_data_indication)(&unpacked_msg);
        free_rx_data_indication(&unpacked_msg);
      }
      break;
      case NFAPI_NR_PHY_MSG_TYPE_CRC_INDICATION: {
        nfapi_nr_crc_indication_t unpacked_msg = {.header.message_id = msgId,
                                                  .header.message_length = msgSize};
        if (wls_vnf_p7_config->unpack_func(msg,
                                       msgSize + NFAPI_NR_P7_HEADER_LENGTH,
                                       &unpacked_msg,
                                       msgSize + NFAPI_NR_P7_HEADER_LENGTH,
                                       0) != 0) {
          printf("Failure unpacking, or dummy message, ignoring\n");
          break;
        }
        DevAssert(wls_vnf_p7_config->nr_crc_indication != NULL);
        (wls_vnf_p7_config->nr_crc_indication)(&unpacked_msg);
        free_crc_indication(&unpacked_msg);
      }
      break;
      case NFAPI_NR_PHY_MSG_TYPE_UCI_INDICATION: {
        nfapi_nr_uci_indication_t unpacked_msg = {.header.message_id = msgId,
                                                  .header.message_length = msgSize};
        if (wls_vnf_p7_config->unpack_func(msg,
                                       msgSize + NFAPI_NR_P7_HEADER_LENGTH,
                                       &unpacked_msg,
                                       msgSize + NFAPI_NR_P7_HEADER_LENGTH,
                                       0) != 0) {
          printf("Failure unpacking, or dummy message, ignoring\n");
          break;
        }
        DevAssert(wls_vnf_p7_config->nr_uci_indication != NULL);
        (wls_vnf_p7_config->nr_uci_indication)(&unpacked_msg);
        free_uci_indication(&unpacked_msg);
      }
      case NFAPI_NR_PHY_MSG_TYPE_SRS_INDICATION: {
        nfapi_nr_srs_indication_t unpacked_msg = {.header.message_id = msgId,
                                                  .header.message_length = msgSize};
        if (wls_vnf_p7_config->unpack_func(msg,
                                       msgSize + NFAPI_NR_P7_HEADER_LENGTH,
                                       &unpacked_msg,
                                       msgSize + NFAPI_NR_P7_HEADER_LENGTH,
                                       0) != 0) {
          printf("Failure unpacking, or dummy message, ignoring\n");
          break;
        }
        DevAssert(wls_vnf_p7_config->nr_srs_indication != NULL);
        (wls_vnf_p7_config->nr_srs_indication)(&unpacked_msg);
        free_srs_indication(&unpacked_msg);
      }
      break;
      case NFAPI_NR_PHY_MSG_TYPE_RACH_INDICATION: {
        nfapi_nr_rach_indication_t unpacked_msg = {.header.message_id = msgId,
                                                   .header.message_length = msgSize};
        if (wls_vnf_p7_config->unpack_func(msg,
                                       msgSize + NFAPI_NR_P7_HEADER_LENGTH,
                                       &unpacked_msg,
                                       msgSize + NFAPI_NR_P7_HEADER_LENGTH,
                                       0) != 0) {
          printf("Failure unpacking, or dummy message, ignoring\n");
          break;
        }
        DevAssert(wls_vnf_p7_config->nr_rach_indication != NULL);
        (wls_vnf_p7_config->nr_rach_indication)(&unpacked_msg);
        free_rach_indication(&unpacked_msg);
      }
      break;
      default:
        printf("UNKNOWN MESSAGE ID 0x%02x\n", msgId);
        break;
    }
  }
}

static void procPhyMessages(uint32_t msg_size, void *msg_buf, uint16_t msg_id)
{
  // Should be able to be casted into already present fapi p5 header format, to fix, dependent on changes on L2 to follow
  // fapi_message_header_t in nr_fapi.h
  NFAPI_TRACE(NFAPI_TRACE_DEBUG, "[VNF] Received Msg ID 0x%02x\n", msg_id);
  if (msg_buf == NULL || _vnf == NULL) {
    AssertFatal(msg_buf != NULL && _vnf != NULL, "%s: NULL parameters\n", __FUNCTION__);
  }
  switch (msg_id) {
    case NFAPI_NR_PHY_MSG_TYPE_PNF_PARAM_RESPONSE:
      vnf_nr_handle_pnf_param_response(msg_buf, msg_size + NFAPI_NR_P5_HEADER_LENGTH, config,0);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_PNF_CONFIG_RESPONSE:
      vnf_nr_handle_pnf_config_response(msg_buf, msg_size + NFAPI_NR_P5_HEADER_LENGTH, config,0);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_PNF_START_RESPONSE:
      vnf_nr_handle_pnf_start_response(msg_buf, msg_size + NFAPI_NR_P5_HEADER_LENGTH, config,0);
      break;

    case NFAPI_PNF_STOP_RESPONSE:
      vnf_handle_pnf_stop_response(msg_buf, msg_size + NFAPI_NR_P5_HEADER_LENGTH, config,0);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_PARAM_RESPONSE:
      vnf_nr_handle_param_response(msg_buf, msg_size + NFAPI_NR_P5_HEADER_LENGTH, config,0);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_CONFIG_RESPONSE:
      vnf_nr_handle_config_response(msg_buf, msg_size + NFAPI_NR_P5_HEADER_LENGTH, config,0);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_START_RESPONSE:
      vnf_nr_handle_start_response(msg_buf, msg_size + NFAPI_NR_P5_HEADER_LENGTH, config,0);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_STOP_RESPONSE:
      vnf_handle_stop_response(msg_buf, msg_size + NFAPI_NR_P5_HEADER_LENGTH, config,0);
      break;
    case NFAPI_NR_PHY_MSG_TYPE_DL_TTI_REQUEST

      ...
    NFAPI_NR_PHY_MSG_TYPE_RACH_INDICATION : {
        wls_vnf_nr_handle_p7_messages(msg_size, msg_buf, msg_id);
        break;
      }
    default:
      break;
  }
}

int wls_fapi_nr_vnf_start(nfapi_vnf_config_t *cfg)
{
  config = cfg;

  if (config == 0) {
    return -1;
  }
  _vnf = (vnf_t *)(config);

  // init WLS connection
  if (vnf_wls_init() == 0) {
    return -1;
  }
  _vnf->terminate = 0;

  // Connect to the PNF
  if (config->pnf_nr_connection_indication != 0) {
    (config->pnf_nr_connection_indication)(config, 0);
  }

  /*if(config->nr_param_resp != 0)
  {
    (config->nr_param_resp)(config,0, NULL);
  }*/

  /* VNF receive loop */
  /* Number of Memory blocks to get */
  uint32_t msgSize;
  uint16_t msgType;
  uint16_t flags;
  uint32_t i = 0;
  PWLS_MAC_CTX pWls = wls_mac_get_ctx();
  while (_vnf->terminate == 0) {
    int numMsgToGet = WLS_Wait(pWls->hWls);
    if (numMsgToGet <= 0) {
      continue;
    }
    while (numMsgToGet--) {
      p_fapi_api_queue_elem_t msg = NULL;
      const uint64_t msg_PA = WLS_Get(pWls->hWls, &msgSize, &msgType, &flags);
      if (msg_PA != 0) {
        i++;
        void *msg_VA = WLS_PA2VA(pWls->hWls, msg_PA);
        msg = (p_fapi_api_queue_elem_t)msg_VA;

        if (msg->msg_type != FAPI_VENDOR_MSG_HEADER_IND) {
          uint8_t *msgt = (uint8_t *)(msg + 1);
          /*for (int x = 0; x < NFAPI_HEADER_LENGTH+10; ++x) {
            printf("0x%02x ", msgt[x]);
          }*/
          uint16_t msg_id = msgt[3] | (msgt[2] << 8);
          //printf("\n ID 0x%02x\n", msg_id);
          uint32_t len = msgt[7] | (msgt[6] << 8) | (msgt[5] << 16) | (msgt[4] << 24);
          //printf("\n LEN 0x%04x\n", len);
          procPhyMessages(len, (void *)(msg + 1), msg_id);
        }
        // Return the block to the L1 after getting the message
        WLS_EnqueueBlock(pWls->hWls, msg_PA);
      } else {
        NFAPI_TRACE(NFAPI_TRACE_ERROR, "\n[PHY] MAC2PHY WLS Get Error for msg %d\n", i);
        break;
      }
      if (flags & WLS_TF_FIN) {
        // Don't return, the messages responses will be handled in procPhyMessages
      }
    }
  }
  return 1;
}


int wls_vnf_nr_pack_and_send_p5_message(vnf_t *vnf, nfapi_nr_p4_p5_message_header_t *msg, uint32_t msg_len)
{
  int packed_len = vnf->_public.pack_func(msg,
                                            msg_len,
                                            vnf->tx_message_buffer,
                                            sizeof(vnf->tx_message_buffer),
                                            &vnf->_public.codec_config);

  if (packed_len < 0) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "nfapi_p5_message_pack failed (%d)\n", packed_len);
    return -1;
  }

  //printf("fapi_nr_p5_message_pack succeeded having packed %d bytes\n", packed_len);
  //printf("in msg, msg_len is %d\n", msg->message_length);
  PWLS_MAC_CTX pWls = wls_mac_get_ctx();

  int num_avail_blocks = WLS_NumBlocks(pWls->hWls);
  printf("num_avail_blocks is %d\n", num_avail_blocks);
  while (num_avail_blocks < 2) {
    num_avail_blocks = WLS_NumBlocks(pWls->hWls);
    printf("num_avail_blocks is %d\n", num_avail_blocks);
  }
  int retval = wls_send_fapi_msg(pWls, msg->message_id, packed_len, vnf->tx_message_buffer);
  if (retval) {
    printf("wls_send_fapi_msg success\n");
  }
  return retval;
}


int wls_vnf_nr_pack_and_send_p7_message(nfapi_nr_p7_message_header_t *msg)
{
  int packed_len = wls_vnf_p7_config->pack_func(msg,
                                            _vnf->tx_message_buffer,
                                            sizeof(_vnf->tx_message_buffer),
                                            NULL);

  if (packed_len < 0) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "fapi_p7_message_pack failed (%d)\n", packed_len);
    return -1;
  }

  //printf("fapi_nr_p7_message_pack succeeded having packed %d bytes\n", packed_len);
  //printf("in msg, msg_len is %d\n", msg->message_length);
  PWLS_MAC_CTX pWls = wls_mac_get_ctx();

  int retval = wls_send_fapi_msg(pWls, msg->message_id, packed_len, _vnf->tx_message_buffer);
  return retval;
}
