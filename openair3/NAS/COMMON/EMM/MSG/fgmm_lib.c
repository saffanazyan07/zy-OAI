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

#include "fgmm_lib.h"
#include <string.h>
#include <arpa/inet.h> // For htons and ntohs
#include <stdlib.h> // For malloc and free
#include "openair3/NAS/COMMON/NR_NAS_defs.h"

// define for both PDU Session Status and PDU session reactivation result IEs
#define MIN_PDU_SESSION_CONTENTS_LEN 2
#define MAX_PDU_SESSION_CONTENTS_LEN 32

/**
 * Encode PDU Session Status and PDU session reactivation result (optional IEs)
 */
int encode_pdu_session_ie(uint8_t *buffer, fgs_service_IEI_t iei, const uint8_t *psi)
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
int decode_pdu_session_ie(uint8_t *psi, const uint8_t *buffer)
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

int encode_gprs_timer_ie(uint8_t *buf, fgs_service_IEI_t iei, const uint8_t val)
{
  int encoded = 0;
  buf[encoded++] = iei;
  buf[encoded++] = 1; // 1 octet
  buf[encoded++] = val;
  return encoded;
}
