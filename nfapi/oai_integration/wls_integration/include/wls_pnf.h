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
/*! \file nfapi/oai_integration/wls_integration/wls_pnf.h
 * \brief
 * \author Ruben S. Silva
 * \date 2024
 * \version 0.1
 * \company OpenAirInterface Software Alliance
 * \email: contact@openairinterface.org, rsilva@allbesmart.pt
 * \note
 * \warning
 */

#ifndef OPENAIRINTERFACE_WLS_PNF_H
#define OPENAIRINTERFACE_WLS_PNF_H
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <rte_eal.h>
#include <rte_cfgfile.h>
#include <rte_string_fns.h>
#include <rte_common.h>
#include <rte_string_fns.h>
#include <rte_lcore.h>
#include <rte_debug.h>
#include <rte_launch.h>
#include <hugetlbfs.h>
#include "wls_lib.h"
#include "nfapi_pnf_interface.h"
#include "nfapi/open-nFAPI/fapi/inc/p5/nr_fapi_p5_utils.h"
#include "nfapi/open-nFAPI/fapi/inc/p7/nr_fapi_p7_utils.h"
#include "pnf.h"

/* ----- WLS Operation --- */
#define FAPI_VENDOR_MSG_HEADER_IND                          0x1A
// Linked list header present at the top of all messages
typedef struct _fapi_api_queue_elem {
  struct _fapi_api_queue_elem *p_next;
  // p_tx_data_elm_list used for TX_DATA.request processing
  struct _fapi_api_queue_elem *p_tx_data_elm_list;
  uint8_t msg_type;
  uint8_t num_message_in_block;
  uint32_t msg_len;
  uint32_t align_offset;
  uint64_t time_stamp;
} fapi_api_queue_elem_t,
    *p_fapi_api_queue_elem_t;

/* ----------------------- */
typedef struct {
  uint8_t num_msg;
  uint8_t opaque_handle;
  uint16_t message_id;
  uint32_t message_length;
} fapi_msg_header;
int wls_pnf_nr_pack_and_send_p5_message(pnf_t* pnf, nfapi_p4_p5_message_header_t* msg, uint32_t msg_len);
int wls_pnf_nr_pack_and_send_p7_message(nfapi_p7_message_header_t * msg);
void *wls_fapi_pnf_nr_start_thread(void *ptr);
int wls_fapi_nr_pnf_start();
void wls_set_p7_config(nfapi_pnf_p7_config_t *p7_config);
#endif // OPENAIRINTERFACE_WLS_PNF_H
