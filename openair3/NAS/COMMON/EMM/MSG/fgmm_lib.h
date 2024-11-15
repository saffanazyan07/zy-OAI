/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.1 (the "License"); you may not use this file
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

#ifndef FGS_LIB_H
#define FGS_LIB_H

#include <stdint.h>
#include "openair3/NAS/COMMON/NR_NAS_defs.h"

// define for both PDU Session Status and PDU session reactivation result IEs
#define MIN_PDU_SESSION_CONTENTS_LEN 2
#define MIN_EAP_MSG_LEN 7
#define MAX_PDU_SESSION_CONTENTS_LEN 32
#define MAX_NUM_PSI 16
#define MAX_NUM_PDU_ERRORS 256
#define EAP_MSG_MAX_LEN 1503

typedef struct {
  uint16_t len;
  uint8_t msg[EAP_MSG_MAX_LEN];
} eap_msg_t;

typedef enum { PDU_SESSION_INACTIVE = 0, PDU_SESSION_ACTIVE } PSI_status_t;

typedef enum { REACTIVATION_SUCCESS = 0, REACTIVATION_FAILED } PSI_reactivation_t;

#define NAS_SERVICE_IEI(IEI_DEF)                          \
  IEI_DEF(IEI_PDU_SESSION_STATUS, 0x50)                   \
  IEI_DEF(IEI_PDU_SESSION_REACT_RESULT, 0x26)             \
  IEI_DEF(IEI_PDU_SESSION_REACT_RESULT_ERROR_CAUSE, 0x72) \
  IEI_DEF(IEI_EAPMSG, 0x78)                               \
  IEI_DEF(IEI_T3448_VALUE, 0x6B)  \
  IEI_DEF(IEI_T3446_VALUE, 0x5F)  

// Enum for Service Accept/Reject IEIs
typedef enum { NAS_SERVICE_IEI(TO_ENUM) } fgs_service_IEI_t;

static const text_info_t sa_iei_s[] = {NAS_SERVICE_IEI(TO_TEXT)};

int encode_pdu_session_ie(uint8_t *buffer, fgs_service_IEI_t iei, const uint8_t *psi);
int decode_pdu_session_ie(uint8_t *psi, const uint8_t *buffer);
int encode_gprs_timer_ie(uint8_t *buf, fgs_service_IEI_t iei, const uint8_t val);
int encode_eap_msg(uint8_t *buffer, const eap_msg_t *msg);
int decode_eap_msg(eap_msg_t *msg, const uint8_t *buffer, const uint8_t buf_len);

#endif /* FGS_LIB_H */
