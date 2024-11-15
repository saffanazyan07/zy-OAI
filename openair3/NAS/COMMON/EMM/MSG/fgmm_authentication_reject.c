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

#include "fgmm_authentication_reject.h"
#include <string.h>
#include <arpa/inet.h> // For htons and ntohs
#include <stdlib.h> // For malloc and free
#include "fgmm_lib.h"

#define MIN_AUTH_REJECT_LEN 3

/**
 * @brief Encode Authentication Reject (8.2.5 of 3GPP TS 24.501)
 */
int encode_fgmm_auth_reject(uint8_t *buffer, const fgmm_auth_reject_msg_t *msg, uint32_t len)
{
  uint32_t encoded = 0;

  const eap_msg_t *eap_msg = &msg->eap_msg;
  if (eap_msg->len > 0) {
    return encode_eap_msg(buffer, eap_msg);
  }
  return encoded;
}

/**
 * @brief Decode Authentication Reject (8.2.5 of 3GPP TS 24.501)
 *        the message contains only optional IEIs (EAP MSG)
 */
int decode_fgmm_auth_reject(fgmm_auth_reject_msg_t *msg, const uint8_t *buffer, uint32_t len)
{
  if (len < MIN_AUTH_REJECT_LEN) {
    // No optional IEIs present
    return -1;
  }
  uint32_t decoded = 0;
  // Decode the IEI
  uint8_t iei = buffer[decoded++];
  if (iei == IEI_EAPMSG) {
    return decode_eap_msg(&msg->eap_msg, &buffer[decoded], len);
  }
  PRINT_NAS_ERROR("Expected EAP MSG but it is not present");
  return -1;
}
