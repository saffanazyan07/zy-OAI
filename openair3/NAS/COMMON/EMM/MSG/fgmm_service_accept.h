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

#ifndef FGS_SERVICE_ACCEPT_H
#define FGS_SERVICE_ACCEPT_H

#include <stdint.h>
#include "openair3/NAS/COMMON/NR_NAS_defs.h"

#define MAX_NUM_PSI 16
#define MAX_NUM_PDU_ERRORS 256

#define FOREACH_SA_IEI(IEI_DEF)                           \
  IEI_DEF(IEI_PDU_SESSION_STATUS, 0x50)                   \
  IEI_DEF(IEI_PDU_SESSION_REACT_RESULT, 0x26)             \
  IEI_DEF(IEI_PDU_SESSION_REACT_RESULT_ERROR_CAUSE, 0x72) \
  IEI_DEF(IEI_EAPMSG, 0x78)                               \
  IEI_DEF(IEI_T3448_VALUE, 0x6B)

// Enum for Service Accept IEIs
typedef enum { FOREACH_SA_IEI(TO_ENUM) } Service_Accept_IEI_t;

static const text_info_t sa_iei_s[] = {FOREACH_SA_IEI(TO_TEXT)};

typedef enum { PDU_SESSION_INACTIVE = 0, PDU_SESSION_ACTIVE } PSI_status_t;

typedef enum { REACTIVATION_SUCCESS = 0, REACTIVATION_FAILED } PSI_reactivation_t;

typedef struct {
  uint8_t pdu_session_id;
  uint8_t cause;
} pdu_error_cause_t;

typedef struct {
  // PDU Session Status (O)
  uint8_t psi_status[MAX_NUM_PSI];
  uint8_t num_psi_status;

  // PDU Session Reactivation Result (O)
  uint8_t psi_res[MAX_NUM_PSI];
  uint8_t num_psi_res;

  // PDU Session Reactivation Result Error Cause (O)
  pdu_error_cause_t cause[MAX_NUM_PDU_ERRORS];
  uint16_t num_errors;

  // T3448 Value (O)
  uint8_t *t3448_value;
} fgs_service_accept_msg_t;

int decode_fgs_service_accept(fgs_service_accept_msg_t *msg, const uint8_t *buffer, uint32_t len);
int encode_fgs_service_accept(uint8_t *buffer, const fgs_service_accept_msg_t *msg, uint32_t len);
void free_fgs_service_accept(fgs_service_accept_msg_t *msg);

#endif /* FGS_SERVICE_ACCEPT_H */
