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

// define for both PDU Session Status and PDU session reactivation result IEs
#define MIN_PDU_SESSION_CONTENTS_LEN 2
#define MAX_PDU_SESSION_CONTENTS_LEN 32

/**
 * Encode PDU Session Status and PDU session reactivation result (optional IEs)
 */
static int encode_pdu_session_ie(uint8_t *buffer, Service_Accept_IEI_t iei, const uint8_t *psi)
{
  int encoded = 0;
  buffer[encoded++] = iei;
  buffer[encoded++] = MIN_PDU_SESSION_CONTENTS_LEN; // no spare octets

  // Initialize the PDU session status octets to zero
  buffer[encoded] = 0x00;
  buffer[encoded + 1] = 0x00;

  int bit_on = iei == IEI_PDU_SESSION_STATUS ? PDU_SESSION_ACTIVE : REACTIVATION_FAILED;

  // Set each PSI bit according to the psi array
  for (uint8_t i = 0; i < MAX_NUM_PSI; i++) {
    if (psi[i]) {
      if (i < 8) {
        // Octet 3 (PSI 1-7), PSI(0) bit is zero
        buffer[encoded] |= ((psi[i] == bit_on) << (i + 1));
      } else {
        // Octet 4 (PSI 8-15)
        buffer[encoded + 1] |= ((psi[i] == bit_on) << (i - 8));
      }
    }
  }
  encoded += 2;
  return encoded;
}

/**
 * Decode PDU Session Status and PDU session reactivation result (optional IEs)
 */
static int decode_pdu_session_ie(uint8_t *psi, const uint8_t *buffer)
{
  int decoded = 0;

  int iei = buffer[decoded++];
  if (iei != IEI_PDU_SESSION_STATUS && iei != IEI_PDU_SESSION_REACT_RESULT) {
    PRINT_NAS_ERROR("wrong IEI %d in decode_pdu_session_ie\n", iei);
    return -1;
  }

  uint8_t pdu_session_status_len = buffer[decoded++];
  if (pdu_session_status_len > MAX_PDU_SESSION_CONTENTS_LEN) {
    PRINT_NAS_ERROR("decoded length is out of bound for IEI %d\n", iei);
    return -1;
  }

  uint8_t octet3 = buffer[decoded++];
  uint8_t octet4 = buffer[decoded++];
  // Decode PSI bits from octet 3 (PSI 1-7)
  for (uint8_t i = 0; i < 8; i++) {
    psi[i] = (octet3 >> (i + 1)) & 0x01;
  }
  // Decode PSI bits from octet 4 (PSI 8-15)
  for (uint8_t i = 8; i < MAX_NUM_PSI; i++) {
    psi[i] = (octet4 >> (i - 8)) & 0x01;
  }

  return decoded;
}

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
