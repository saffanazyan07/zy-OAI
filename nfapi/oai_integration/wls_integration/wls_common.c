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

#include "wls_common.h"


uint8_t wls_send_fapi_msg(PWLS_MAC_CTX pWls, const uint16_t message_id, const int packed_len, const uint8_t *message )
{
  /* get PA blocks for header and msg */
  uint64_t pa_hdr = WLS_DequeueBlock(pWls->hWls);
  AssertFatal(pa_hdr, "WLS_DequeueBlock failed for pa_hdr\n");
  uint64_t pa_msg = WLS_DequeueBlock(pWls->hWls);
  AssertFatal(pa_msg, "WLS_DequeueBlock failed for pa_msg\n");
  p_fapi_api_queue_elem_t  headerElem = WLS_PA2VA(pWls->hWls, pa_hdr);// WLS_Alloc(pWls->hWls, (sizeof(fapi_api_queue_elem_t) + 2));// (p_fapi_api_queue_elem_t)wls_mac_alloc_buffer((sizeof(fapi_api_queue_elem_t) + 2), 0);
  AssertFatal(headerElem, "WLS_PA2VA failed for headerElem\n");
  p_fapi_api_queue_elem_t fapiMsgElem = WLS_PA2VA(pWls->hWls, pa_msg);//WLS_Alloc(pWls->hWls, (sizeof(fapi_api_queue_elem_t)+packed_len+NFAPI_HEADER_LENGTH));//  (p_fapi_api_queue_elem_t)wls_mac_alloc_buffer((sizeof(fapi_api_queue_elem_t)+packed_len+NFAPI_HEADER_LENGTH), 0);
  AssertFatal(fapiMsgElem, "WLS_PA2VA failed for fapiMsgElem\n");
  FILL_FAPI_LIST_ELEM(fapiMsgElem, NULL, message_id, 1,  packed_len+NFAPI_HEADER_LENGTH);
  memcpy((uint8_t *)(fapiMsgElem +1),message,packed_len+NFAPI_HEADER_LENGTH);
  //fapi_message_header_t msgHeader = {.num_msg = 1, .opaque_handle = 0 ,.message_length = msg->message_length, .message_id = msg->message_id};
  uint8_t wls_header[] = {1,0}; // num_messages ,  opaque_handle
  if (NFAPI_MODE == NFAPI_MODE_VNF) {
    // Use the opaque handle to signal to our PNF to not progress the FAPI PNF state machine
    wls_header[1] = 0xff;
  }
  FILL_FAPI_LIST_ELEM(headerElem, fapiMsgElem, FAPI_VENDOR_MSG_HEADER_IND, 1, 2);
  memcpy((uint8_t *)(headerElem+1),wls_header, 2);
  uint8_t retval =  wls_msg_send(pWls->hWls,headerElem);
  if (NFAPI_MODE == NFAPI_MODE_VNF) {
    // Return the sent blocks
    WLS_EnqueueBlock(pWls->hWls, pa_msg);
    WLS_EnqueueBlock(pWls->hWls, pa_hdr);
  }
  return retval;
}

uint8_t wls_msg_send(PWLS_MAC_CTX pWls, void *msg)
{
  uint32_t msgLen = 0;
  p_fapi_api_queue_elem_t currMsg = NULL;

  if (msg) {
    currMsg = (p_fapi_api_queue_elem_t)msg;
    msgLen = currMsg->msg_len + sizeof(fapi_api_queue_elem_t);
    if (currMsg->p_next == NULL) {
      printf("\nERROR  -->  LWR MAC : There cannot be only one block to send");
      return false;
    }
    /* Sending first block */
    /* Call WLS_VA2PA to convert struct pointer to physical address */
    if (WLS_Put(pWls, WLS_VA2PA(pWls, currMsg), msgLen, currMsg->msg_type, WLS_SG_FIRST) != 0) {
      printf("\nERROR  -->  LWR MAC : Failure in sending message to PHY");
      return false;
    }
    /* Advance to the next message block */
    currMsg = currMsg->p_next;
    while (currMsg) {
      /* Sending the next msg */
      msgLen = currMsg->msg_len + sizeof(fapi_api_queue_elem_t);
      /* Send the block with appropriate flags */
      const unsigned short flags = currMsg->p_next != NULL ? WLS_SG_NEXT : WLS_SG_LAST;
      if (WLS_Put(pWls, WLS_VA2PA(pWls, currMsg), msgLen, currMsg->msg_type, flags) != 0) {
        printf("\nERROR  -->  LWR MAC : Failure in sending message to PHY");
        return false;
      }
      if(flags == WLS_SG_LAST) {
        /**Need to determine if it is to break using the flags,
         *since currMsg->p_next can become 0xffffffffffffffff after WLS_VA2PA*/
        break;
      }
      /* No need for ternary operator, if p_next is NULL, that's what we want */
      /* currMsg = currMsg->p_next == NULL ? NULL : currMsg->p_next;*/
      currMsg = currMsg->p_next;
    }
  } else {
    //msg doesn't exist
    printf("\nERROR  -->  LWR MAC : msg is NULL");
    return false;
  }
  return true;
}
