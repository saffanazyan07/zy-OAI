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
#include "nr_fapi.h"

bool isFAPIMessageIDValid(const uint16_t id)
{
  // SCF 222.10.04 Table 3-5 PHY API message types
  return (id >= NFAPI_NR_PHY_MSG_TYPE_PARAM_REQUEST && id <= 0xFF) || id == NFAPI_NR_PHY_MSG_TYPE_START_RESPONSE
         || id == NFAPI_NR_PHY_MSG_TYPE_UL_NODE_SYNC || id == NFAPI_NR_PHY_MSG_TYPE_DL_NODE_SYNC
         || id == NFAPI_NR_PHY_MSG_TYPE_TIMING_INFO;
}

int fapi_nr_message_header_unpack(uint8_t **pMessageBuf,
                                  uint32_t messageBufLen,
                                  void *pUnpackedBuf,
                                  uint32_t unpackedBufLen,
                                  nfapi_p4_p5_codec_config_t *config)
{
  uint8_t **pReadPackedMessage = pMessageBuf;
  nfapi_nr_p4_p5_message_header_t *header = pUnpackedBuf;
  fapi_message_header_t fapi_msg = {0};

  if (pMessageBuf == NULL || pUnpackedBuf == NULL || messageBufLen < NFAPI_HEADER_LENGTH
      || unpackedBufLen < sizeof(fapi_message_header_t)) {
    return -1;
  }
  uint8_t *end = *pMessageBuf + messageBufLen;
  // process the header
  int result =
      (pull8(pReadPackedMessage, &fapi_msg.num_msg, end) && pull8(pReadPackedMessage, &fapi_msg.opaque_handle, end)
       && pull16(pReadPackedMessage, &header->message_id, end) && pull32(pReadPackedMessage, &header->message_length, end));
  return (result);
}
