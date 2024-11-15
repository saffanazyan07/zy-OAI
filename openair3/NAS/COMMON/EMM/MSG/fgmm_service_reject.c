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

#include "fgmm_service_reject.h"
#include <string.h>
#include <arpa/inet.h> // For htons and ntohs
#include <stdlib.h> // For malloc and free
#include "fgmm_lib.h"

/**
 * @brief Encode Service Reject (8.2.18 of 3GPP TS 24.501)
 */
int encode_fgs_service_reject(const fgs_service_reject_msg_t *msg, uint8_t *buffer, uint32_t len)
{
  uint32_t encoded = 0;
  int enc = 0;

  if (len < 1)
    return -1;

  // 5GMM cause (M)
  buffer[encoded++] = msg->cause;

  // PDU Session Status (O)
  if (msg->num_psi_status) {
    if ((enc = encode_pdu_session_ie(buffer + encoded, IEI_PDU_SESSION_STATUS, msg->psi_status)) < 0) {
      return enc;
    }
    encoded += enc;
  }

  // T3446 Timer (O)
  if (msg->t3446_value) {
    if ((enc = encode_gprs_timer_ie(buffer + encoded, IEI_T3446_VALUE, *msg->t3446_value)) < 0) {
      return enc;
    }
    encoded += enc;
  }

  // T3448 Timer (O)
  if (msg->t3448_value) {
    if ((enc = encode_gprs_timer_ie(buffer + encoded, IEI_T3448_VALUE, *msg->t3448_value)) < 0) {
      return enc;
    }
    encoded += enc;
  }

  return encoded;
}

/**
 * @brief Decode Service Reject (8.2.18 of 3GPP TS 24.501)
 */
int decode_fgs_service_reject(fgs_service_reject_msg_t *msg, const uint8_t *buffer, uint32_t len)
{
  uint32_t decoded = 0;
  int dec = 0;

  if (len < 1)
    return -1; // nothing to decode

  // Cause (M)
  msg->cause = buffer[decoded++];

  // Decode Optional IEs
  while (decoded < len) {
    uint8_t iei = buffer[decoded];

    switch (iei) {
      case IEI_PDU_SESSION_STATUS:
        if ((dec = decode_pdu_session_ie(msg->psi_status, buffer + decoded)) < 0) {
          return dec;
        }
        decoded += dec;
        break;

      case IEI_T3448_VALUE:
        // Skip IEI and length
        decoded += 2;
        msg->t3448_value = (uint8_t *)malloc(sizeof(*msg->t3448_value));
        *msg->t3448_value = buffer[decoded++];
        break;

      case IEI_T3446_VALUE:
        // Skip IEI and length
        decoded += 2;
        msg->t3446_value = (uint8_t *)malloc(sizeof(*msg->t3446_value));
        *msg->t3446_value = buffer[decoded++];
        break;

      default:
        PRINT_NAS_ERROR("unknown IEI %d\n", iei);
        return -1;
    }
  }

  return decoded;
}

void free_fgs_service_reject(fgs_service_reject_msg_t *msg)
{
  free(msg->t3446_value);
  free(msg->t3448_value);
}
