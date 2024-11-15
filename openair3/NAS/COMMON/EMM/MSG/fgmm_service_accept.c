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

#include "fgmm_service_accept.h"
#include <string.h>
#include <arpa/inet.h> // For htons and ntohs
#include <stdlib.h> // For malloc and free

int encode_fgs_service_accept(uint8_t *buffer, const fgs_service_accept_msg_t *msg, uint32_t len)
{
  uint32_t encoded = 0;
  int enc = 0;

  if (len < 1)
    return -1;

  // Encode PDU Session Status (O)
  if (msg->num_psi_status) {
    if ((enc = encode_pdu_session_ie(buffer + encoded, IEI_PDU_SESSION_STATUS, msg->psi_status)) < 0) {
      return enc;
    }
    encoded += enc;
  }

  // Encode PDU Session Reactivation Result (O)
  if (msg->num_psi_res) {
    if ((enc = encode_pdu_session_ie(buffer + encoded, IEI_PDU_SESSION_REACT_RESULT, msg->psi_res)) < 0) {
      return enc;
    }
    encoded += enc;
  }

  // Encode PDU Session Reactivation Result Error Cause (O)
  if (msg->num_errors) {
    buffer[encoded++] = IEI_PDU_SESSION_REACT_RESULT_ERROR_CAUSE;
    // encode length of PDU session reactivation result error cause
    uint16_t error_len = msg->num_errors * sizeof(msg->cause[0]);
    if (error_len > MAX_NUM_PDU_ERRORS) {
      PRINT_NAS_ERROR("encoded length is out of bound (IEI_PDU_SESSION_REACT_RESULT_ERROR_CAUSE)\n");
      return -1;
    }
    uint16_t n_len = htons(error_len);
    memcpy(&buffer[encoded], &n_len, sizeof(n_len)); // length payload PDU Session Status
    encoded += sizeof(error_len);
    // encode PDU Session IDs and Causes
    for (int i = 0; i < msg->num_errors; i++) {
      buffer[encoded++] = msg->cause[i].pdu_session_id;
      buffer[encoded++] = msg->cause[i].cause;
    }
  }

  // Encode T3448 Timer (O)
  if (msg->t3448_value) {
    buffer[encoded++] = IEI_T3448_VALUE;
    buffer[encoded++] = 1; // 1 octet
    buffer[encoded++] = *msg->t3448_value;
  }

  return encoded;
}

int decode_fgs_service_accept(fgs_service_accept_msg_t *msg, const uint8_t *buffer, uint32_t len)
{
  uint32_t decoded = 0;
  int dec = 0;

  if (len < 1)
    return -1; // nothing to decode

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

      case IEI_PDU_SESSION_REACT_RESULT:
        if ((dec = decode_pdu_session_ie(msg->psi_res, buffer + decoded)) < 0) {
          return dec;
        }
        decoded += dec;
        break;

      case IEI_PDU_SESSION_REACT_RESULT_ERROR_CAUSE:
        // Skip IEI
        decoded++;
        // Decode the length of the IE
        uint16_t tmp;
        memcpy(&tmp, &buffer[decoded], sizeof(tmp));
        uint16_t error_len = ntohs(tmp);
        decoded += sizeof(error_len);

        if (error_len > MAX_NUM_PDU_ERRORS * sizeof(msg->cause[0])) {
          PRINT_NAS_ERROR("IEI_PDU_SESSION_REACT_RESULT_ERROR_CAUSE: decoded length is out of bound\n");
          return -1;
        }

        // Decode each PDU Session ID and Cause
        for (int i = 0; i < error_len; i += 2) {
          msg->cause[msg->num_errors].pdu_session_id = buffer[decoded++];
          msg->cause[msg->num_errors].cause = buffer[decoded++];
          msg->num_errors++;
        }
        break;

      case IEI_T3448_VALUE:
        // Skip IEI and length
        decoded += 2;
        msg->t3448_value = malloc_or_fail(sizeof(*msg->t3448_value));
        *msg->t3448_value = buffer[decoded++];
        break;

      default:
        PRINT_NAS_ERROR("unknown IEI %d\n", iei);
        return -1;
    }
  }

  return decoded;
}

void free_fgs_service_accept(fgs_service_accept_msg_t *msg)
{
  free(msg->t3448_value);
}
