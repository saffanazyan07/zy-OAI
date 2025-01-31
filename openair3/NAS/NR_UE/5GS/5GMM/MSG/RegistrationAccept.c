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

/*! \file RegistrationAccept.c

\brief 5GS registration accept procedures
\author Yoshio INOUE, Masayuki HARADA
\email: yoshio.inoue@fujitsu.com,masayuki.harada@fujitsu.com
\date 2020
\version 0.1
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "conversions.h"
#include "TLVEncoder.h"
#include "TLVDecoder.h"
#include "RegistrationAccept.h"
#include "assertions.h"
#include "fgs_nas_utils.h"

#define IEI_5G_GUTI 0x77

/**
 * @brief Allowed NSSAI from Registration Accept according to 3GPP TS 24.501 Table 8.2.7.1.1
 */
static int decode_nssai_ie(nr_nas_msg_snssai_t *nssai, const uint8_t *buf)
{
  const int length = *buf; // Length of S-NSSAI IE contents (list of S-NSSAIs)
  const uint32_t decoded = length + 1; // Length IE (1 octet)
  buf++;
  int nssai_cnt = 0;
  const uint8_t *end = buf + length;
  while (buf < end) {
    nr_nas_msg_snssai_t *item = nssai + nssai_cnt;
    item->sd = 0xffffff;
    const int item_len = *buf++; // Length of S-NSSAI IE item
    switch (item_len) {
      case 1:
        item->sst = *buf++;
        nssai_cnt++;
        break;

      case 2:
        item->sst = *buf++;
        item->hplmn_sst = *buf++;
        nssai_cnt++;
        break;

      case 4:
        item->sst = *buf++;
        item->sd = 0xffffff & ntoh_int24_buf(buf);
        buf += 3;
        nssai_cnt++;
        break;

      case 5:
        item->sst = *buf++;
        item->sd = 0xffffff & ntoh_int24_buf(buf);
        buf += 3;
        item->hplmn_sst = *buf++;
        nssai_cnt++;
        break;

      case 8:
        item->sst = *buf++;
        item->sd = 0xffffff & ntoh_int24_buf(buf);
        buf += 3;
        item->hplmn_sst = *buf++;
        item->hplmn_sd = 0xffffff & ntoh_int24_buf(buf);
        buf += 3;
        nssai_cnt++;
        break;

      default:
        PRINT_NAS_ERROR("Unknown length in Allowed S-NSSAI list item\n");
        break;
    }
  }
  return decoded;
}

int decode_registration_accept(registration_accept_msg *registration_accept, const uint8_t *buffer, uint32_t len)
{
  int dec = 0;
  const uint8_t *end = buffer + len;

  /* Decoding mandatory fields */
  if ((dec = decode_fgs_registration_result(&registration_accept->fgsregistrationresult, 0, *buffer, len)) < 0)
    return dec;
  buffer += dec;

  if (buffer < end && *buffer == IEI_5G_GUTI) {
    registration_accept->guti = calloc_or_fail(1, sizeof(*registration_accept->guti));
    if ((dec = decode_5gs_mobile_identity(registration_accept->guti, IEI_5G_GUTI, buffer, end - buffer)) < 0) {
      PRINT_NAS_ERROR("Failed to decode 5GS Mobile Identity in Registration Accept\n");
      return -1;
    }
    buffer += dec;
  }

  // Allowed NSSAI (O)
  /* Optional Presence IEs */
  while (buffer < end) {
    const int iei = *buffer++;
    switch (iei) {

      case 0x15: // allowed NSSAI
        dec = decode_nssai_ie(registration_accept->nas_allowed_nssai, buffer);
        buffer += dec;
        break;

      case 0x31: // configured NSSAI
        dec = decode_nssai_ie(registration_accept->config_nssai, buffer);
        buffer += dec;
        break;

      default:
        dec = *buffer++; // content length + 1 byte (Length IE)
        buffer += dec;
        break;
    }
  }
  return len;
}

int encode_registration_accept(const registration_accept_msg *registration_accept, uint8_t *buffer, uint32_t len)
{
  int encoded = 0;

  LOG_FUNC_IN;

  *(buffer + encoded) = encode_fgs_registration_result(&registration_accept->fgsregistrationresult);
  encoded = encoded + 2;

  if (registration_accept->guti) {
    int mi_enc = encode_5gs_mobile_identity(registration_accept->guti, 0x77, buffer + encoded, len - encoded);
    if (mi_enc < 0)
      return mi_enc;
    encoded += mi_enc;
  }

  // todo ,Encoding optional fields
  LOG_FUNC_RETURN(encoded);
}
