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

#include "fgmm_security_mode_reject.h"

/**
 * @brief Encode Security Mode Reject (8.2.18 of 3GPP TS 24.501)
 */
int encode_fgmm_sec_mode_reject(uint8_t *buffer, const fgmm_sec_mode_reject_msg_t *msg, uint32_t len)
{
  uint32_t encoded = 0;

  if (len < 1)
    return -1;

  // 5GMM cause (M)
  buffer[encoded++] = msg->cause;

  return encoded;
}

/**
 * @brief Decode Security Mode Reject (8.2.18 of 3GPP TS 24.501)
 */
int decode_fgmm_sec_mode_reject(fgmm_sec_mode_reject_msg_t *msg, const uint8_t *buffer, uint32_t len)
{
  uint32_t decoded = 0;

  if (len < 1)
    return -1; // nothing to decode

  // Cause (M)
  msg->cause = buffer[decoded++];

  return decoded;
}
