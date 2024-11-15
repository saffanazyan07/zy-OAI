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

int encode_eap_msg(uint8_t *buffer, const eap_msg_t *msg)
{
  int encoded = 0;
  // Encode the IEI
  buffer[encoded++] = IEI_EAPMSG;

  // Encode the length of the contents
  uint16_t len = htons(msg->len);
  memcpy(&buffer[encoded], &len, sizeof(len));
  encoded += sizeof(len);

  // Check length
  int total_length = encoded + msg->len; // IEI (1 byte) + length (2 bytes) + EAP message
  if (total_length < MIN_EAP_MSG_LEN) {
    PRINT_NAS_ERROR("Invalid length: authentication message is too short")
  }
  // Encode the EAP message
  memcpy(&buffer[encoded], msg->msg, msg->len);
  encoded += msg->len;
  return encoded;
}

int decode_eap_msg(eap_msg_t *msg, const uint8_t *buffer, const uint8_t buf_len)
{
  // Decode the length of the EAP message
  uint16_t len = 0;
  int decoded = 0;
  memcpy(&len, &buffer[decoded], sizeof(len));
  decoded += sizeof(len);
  len = ntohs(len);

  // Verify that the remaining buffer length matches the EAP message length
  if (decoded + len > buf_len) {
    PRINT_NAS_ERROR("Buffer length mismatch while decoding EAP message");
    return -1;
  }

  // Verify minimum EAP message length
  if (len < MIN_EAP_MSG_LEN) {
    PRINT_NAS_ERROR("Invalid length: EAP message length is too short");
    return -1;
  }

  // Decode the EAP message
  msg->len = len;
  memcpy(msg->msg, &buffer[decoded], msg->len);
  decoded += len;
  return decoded;
}
