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
 * Author and copyright: Laurent Thomas, open-cells.com
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

#include "e1ap.h"
#include "e1ap_common.h"
#include "gnb_config.h"
#include "openair2/SDAP/nr_sdap/nr_sdap_entity.h"
#include "openair3/UTILS/conversions.h"
#include "openair2/RRC/NR/MESSAGES/asn1_msg.h"
#include "common/openairinterface5g_limits.h"
#include "common/utils/LOG/log.h"
#include "openair2/F1AP/f1ap_common.h"
#include "e1ap_default_values.h"
#include "gtp_itf.h"
#include "openair2/LAYER2/nr_pdcp/cucp_cuup_handler.h"
#include "lib/e1ap_bearer_context_management.h"
#include "lib/e1ap_interface_management.h"

#define E1AP_NUM_MSG_HANDLERS 14
typedef int (*e1ap_message_processing_t)(sctp_assoc_t assoc_id, e1ap_upcp_inst_t *inst, const E1AP_E1AP_PDU_t *message_p);

/**
 * @brief E1AP messages handlers
 */
const e1ap_message_processing_t e1ap_message_processing[E1AP_NUM_MSG_HANDLERS][3] = {

    {0, 0, 0}, /* Reset */
    {0, 0, 0}, /* ErrorIndication */
    {0, 0, 0}, /* privateMessage */
    {e1apCUCP_handle_SETUP_REQUEST, e1apCUUP_handle_SETUP_RESPONSE, e1apCUUP_handle_SETUP_FAILURE}, /* gNBCUUPE1Setup */
    {0, 0, 0}, /* gNBCUCPE1Setup */
    {0, 0, 0}, /* gNBCUUPConfigurationUpdate */
    {0, 0, 0}, /* gNBCUCPConfigurationUpdate */
    {0, 0, 0}, /* E1Release */
    {e1apCUUP_handle_BEARER_CONTEXT_SETUP_REQUEST,
     e1apCUCP_handle_BEARER_CONTEXT_SETUP_RESPONSE,
     e1apCUCP_handle_BEARER_CONTEXT_SETUP_FAILURE}, /* bearerContextSetup */
    {e1apCUUP_handle_BEARER_CONTEXT_MODIFICATION_REQUEST,
     e1apCUCP_handle_BEARER_CONTEXT_MODIFICATION_RESPONSE,
     0}, /* bearerContextModification */
    {0, 0, 0}, /* bearerContextModificationRequired */
    {e1apCUUP_handle_BEARER_CONTEXT_RELEASE_COMMAND, e1apCUCP_handle_BEARER_CONTEXT_RELEASE_COMPLETE, 0}, /* bearerContextRelease */
    {0, 0, 0}, /* bearerContextReleaseRequired */
    {0, 0, 0} /* bearerContextInactivityNotification */
};

const char *const e1ap_direction2String(int e1ap_dir)
{
  static const char *e1ap_direction_String[] = {
      "", /* Nothing */
      "Initiating message", /* initiating message */
      "Successfull outcome", /* successfull outcome */
      "UnSuccessfull outcome", /* successfull outcome */
  };
  return (e1ap_direction_String[e1ap_dir]);
}

static int e1ap_handle_message(instance_t instance, sctp_assoc_t assoc_id, const uint8_t *const data, const uint32_t data_length)
{
  E1AP_E1AP_PDU_t pdu = {0};
  int ret;
  DevAssert(data != NULL);

  if (e1ap_decode_pdu(&pdu, data, data_length) < 0) {
    LOG_E(E1AP, "Failed to decode PDU\n");
    return -1;
  }
  const E1AP_ProcedureCode_t procedureCode = pdu.choice.initiatingMessage->procedureCode;
  /* Checking procedure Code and direction of message */
  if ((procedureCode >= E1AP_NUM_MSG_HANDLERS) || (pdu.present > E1AP_E1AP_PDU_PR_unsuccessfulOutcome)
      || (pdu.present <= E1AP_E1AP_PDU_PR_NOTHING)) {
    LOG_E(E1AP, "[SCTP %d] Either procedureCode %ld or direction %d exceed expected\n", assoc_id, procedureCode, pdu.present);
    ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E1AP_E1AP_PDU, &pdu);
    return -1;
  }

  if (e1ap_message_processing[procedureCode][pdu.present - 1] == NULL) {
    // No handler present. This can mean not implemented or no procedure for eNB
    // (wrong direction).
    LOG_E(E1AP,
          "[SCTP %d] No handler for procedureCode %ld in %s\n",
          assoc_id,
          procedureCode,
          e1ap_direction2String(pdu.present - 1));
    ret = -1;
  } else {
    /* Calling the right handler */
    LOG_D(E1AP, "Calling handler with instance %ld\n", instance);
    ret = (*e1ap_message_processing[procedureCode][pdu.present - 1])(assoc_id, getCxtE1(instance), &pdu);
  }

  ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E1AP_E1AP_PDU, &pdu);
  return ret;
}

static void e1_task_handle_sctp_data_ind(instance_t instance, sctp_data_ind_t *sctp_data_ind)
{
  int result;
  DevAssert(sctp_data_ind != NULL);
  e1ap_handle_message(instance, sctp_data_ind->assoc_id, sctp_data_ind->buffer, sctp_data_ind->buffer_length);
  result = itti_free(TASK_UNKNOWN, sctp_data_ind->buffer);
  AssertFatal(result == EXIT_SUCCESS, "Failed to free memory (%d)!\n", result);
}

int e1ap_send_RESET(bool isCu, sctp_assoc_t assoc_id, E1AP_Reset_t *Reset)
{
  AssertFatal(false, "Not implemented yet\n");
  E1AP_E1AP_PDU_t pdu = {0};
  return e1ap_encode_send(isCu, assoc_id, &pdu, 0, __func__);
}

int e1ap_send_RESET_ACKNOWLEDGE(instance_t instance, E1AP_Reset_t *Reset)
{
  AssertFatal(false, "Not implemented yet\n");
  return -1;
}

int e1ap_handle_RESET(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, E1AP_E1AP_PDU_t *pdu)
{
  AssertFatal(false, "Not implemented yet\n");
  return -1;
}

int e1ap_handle_RESET_ACKNOWLEDGE(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, E1AP_E1AP_PDU_t *pdu)
{
  AssertFatal(false, "Not implemented yet\n");
  return -1;
}

/*
    Error Indication
*/
int e1ap_handle_ERROR_INDICATION(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, E1AP_E1AP_PDU_t *pdu)
{
  AssertFatal(false, "Not implemented yet\n");
  return -1;
}

int e1ap_send_ERROR_INDICATION(instance_t instance, E1AP_ErrorIndication_t *ErrorIndication)
{
  AssertFatal(false, "Not implemented yet\n");
  return -1;
}

void e1ap_send_SETUP_RESPONSE(sctp_assoc_t assoc_id, const e1ap_setup_resp_t *e1ap_setup_resp)
{
  E1AP_E1AP_PDU_t *pdu = encode_e1ap_cuup_setup_response(e1ap_setup_resp);
  e1ap_encode_send(CPtype, assoc_id, pdu, 0, __func__);
}

/**
 * @brief E1 Setup Failure ASN1 messager encoder
 */
static void e1apCUCP_send_SETUP_FAILURE(sctp_assoc_t assoc_id, const e1ap_setup_fail_t *msg)
{
  LOG_D(E1AP, "CU-CP: Encoding E1AP Setup Failure for transac_id %ld...\n", msg->transac_id);
  E1AP_E1AP_PDU_t *pdu = encode_e1ap_cuup_setup_failure(msg);
  e1ap_encode_send(CPtype, assoc_id, pdu, 0, __func__);
}

int e1apCUCP_handle_SETUP_REQUEST(sctp_assoc_t assoc_id, e1ap_upcp_inst_t *inst, const E1AP_E1AP_PDU_t *pdu)
{
  DevAssert(pdu != NULL);
  /* Create ITTI message and send to queue */
  MessageDef *msg_p = itti_alloc_new_message(TASK_CUCP_E1, 0 /*unused by callee*/, E1AP_SETUP_REQ);
  // Decode E1 CU-UP Setup Request
  if (!decode_e1ap_cuup_setup_request(pdu, &E1AP_SETUP_REQ(msg_p))) {
    free_e1ap_cuup_setup_request(&E1AP_SETUP_REQ(msg_p));
    return -1;
  }

  msg_p->ittiMsgHeader.originInstance = assoc_id;

  if (E1AP_SETUP_REQ(msg_p).supported_plmns > 0) {
    itti_send_msg_to_task(TASK_RRC_GNB, 0 /*unused by callee*/, msg_p);
  } else {
    e1ap_cause_t cause = { .type = E1AP_CAUSE_PROTOCOL, .value = E1AP_PROTOCOL_CAUSE_SEMANTIC_ERROR};
    e1ap_setup_fail_t setup_fail = {.transac_id = E1AP_SETUP_REQ(msg_p).transac_id, .cause = cause};
    e1apCUCP_send_SETUP_FAILURE(assoc_id, &setup_fail);
    itti_free(TASK_CUCP_E1, msg_p);
    return -1;
  }

  return 0;
}

int e1apCUUP_handle_SETUP_RESPONSE(sctp_assoc_t assoc_id, e1ap_upcp_inst_t *inst, const E1AP_E1AP_PDU_t *pdu)
{
  // Decode E1 CU-UP Setup Response
  e1ap_setup_resp_t out = {0};
  if (!decode_e1ap_cuup_setup_response(pdu, &out)) {
    free_e1ap_cuup_setup_response(&out);
    return -1;
  }
  long old_transaction_id = inst->cuup.setupReq.transac_id;
  if (old_transaction_id != out.transac_id) {
    LOG_E(E1AP, "Transaction IDs do not match %ld != %ld\n", old_transaction_id, out.transac_id);
    free_e1ap_cuup_setup_response(&out);
    return -1;
  }
  free_e1ap_cuup_setup_response(&out);
  return 0;
}

/**
 * @brief E1 Setup Failure ASN1 messager decoder on CU-UP
 */
int e1apCUUP_handle_SETUP_FAILURE(sctp_assoc_t assoc_id, e1ap_upcp_inst_t *inst, const E1AP_E1AP_PDU_t *pdu)
{
  LOG_D(E1AP, "CU-UP: Decoding E1AP Setup Failure...\n");
  E1AP_GNB_CU_UP_E1SetupFailureIEs_t *ie;
  DevAssert(pdu != NULL);
  E1AP_GNB_CU_UP_E1SetupFailure_t *in = &pdu->choice.unsuccessfulOutcome->value.choice.GNB_CU_UP_E1SetupFailure;
  /* Transaction ID */
  F1AP_FIND_PROTOCOLIE_BY_ID(E1AP_GNB_CU_UP_E1SetupFailureIEs_t, ie, in, E1AP_ProtocolIE_ID_id_TransactionID, true);
  long transaction_id;
  long old_transaction_id = inst->cuup.setupReq.transac_id;
  transaction_id = ie->value.choice.TransactionID;
  if (old_transaction_id != transaction_id)
    LOG_E(E1AP, "Transaction IDs do not match %ld != %ld\n", old_transaction_id, transaction_id);
  E1AP_free_transaction_identifier(transaction_id);

  /* Cause */
  F1AP_FIND_PROTOCOLIE_BY_ID(E1AP_GNB_CU_UP_E1SetupFailureIEs_t, ie, in, E1AP_ProtocolIE_ID_id_Cause, true);

  LOG_E(E1AP, "received E1 Setup Failure, please check the CU-CP output and the CU-UP parameters\n");
  exit(1);
  return 0;
}

static void fill_CONFIGURATION_UPDATE(E1AP_E1AP_PDU_t *pdu)
{
  /* Create */
  /* 0. pdu Type */
  pdu->present = E1AP_E1AP_PDU_PR_initiatingMessage;
  asn1cCalloc(pdu->choice.initiatingMessage, msg);
  msg->procedureCode = E1AP_ProcedureCode_id_gNB_CU_UP_ConfigurationUpdate;
  msg->criticality   = E1AP_Criticality_reject;
  msg->value.present = E1AP_InitiatingMessage__value_PR_GNB_CU_UP_ConfigurationUpdate;
  E1AP_GNB_CU_UP_ConfigurationUpdate_t *out = &pdu->choice.initiatingMessage->value.choice.GNB_CU_UP_ConfigurationUpdate;
  /* mandatory */
  /* c1. Transaction ID (integer value) */
  asn1cSequenceAdd(out->protocolIEs.list, E1AP_GNB_CU_UP_ConfigurationUpdateIEs_t, ieC1);
  ieC1->id                         = E1AP_ProtocolIE_ID_id_TransactionID;
  ieC1->criticality                = E1AP_Criticality_reject;
  ieC1->value.present              = E1AP_GNB_CU_UP_ConfigurationUpdateIEs__value_PR_TransactionID;
  ieC1->value.choice.TransactionID = 0;//get this from stored transaction IDs in CU
  /* mandatory */
  /* c2. Supported PLMNs */
  asn1cSequenceAdd(out->protocolIEs.list, E1AP_GNB_CU_UP_ConfigurationUpdateIEs_t, ieC2);
  ieC2->id = E1AP_ProtocolIE_ID_id_SupportedPLMNs;
  ieC2->criticality = E1AP_Criticality_reject;
  ieC2->value.present = E1AP_GNB_CU_UP_ConfigurationUpdateIEs__value_PR_SupportedPLMNs_List;

  int numSupportedPLMNs = 1;

  for (int i = 0; i < numSupportedPLMNs; i++) {
    asn1cSequenceAdd(ieC2->value.choice.SupportedPLMNs_List.list, E1AP_SupportedPLMNs_Item_t, supportedPLMN);
    /* 5.1 PLMN Identity */
    OCTET_STRING_fromBuf(&supportedPLMN->pLMN_Identity, "OAI", strlen("OAI"));
  }

  /* mandatory */
  /* c3. TNLA to remove list */
  asn1cSequenceAdd(out->protocolIEs.list, E1AP_GNB_CU_UP_ConfigurationUpdateIEs_t, ieC3);
  ieC3->id = E1AP_ProtocolIE_ID_id_GNB_CU_UP_TNLA_To_Remove_List;
  ieC3->criticality = E1AP_Criticality_reject;
  ieC3->value.present = E1AP_GNB_CU_UP_ConfigurationUpdateIEs__value_PR_GNB_CU_UP_TNLA_To_Remove_List;

  int numTNLAtoRemoveList = 1;

  for (int i = 0; i < numTNLAtoRemoveList; i++) {
    asn1cSequenceAdd(ieC2->value.choice.GNB_CU_UP_TNLA_To_Remove_List.list, E1AP_GNB_CU_UP_TNLA_To_Remove_Item_t, TNLAtoRemove);
    TNLAtoRemove->tNLAssociationTransportLayerAddress.present = E1AP_CP_TNL_Information_PR_endpoint_IP_Address;
    TRANSPORT_LAYER_ADDRESS_IPv4_TO_BIT_STRING(1234, &TNLAtoRemove->tNLAssociationTransportLayerAddress.choice.endpoint_IP_Address); // TODO: correct me
  }
}

/*
  E1 configuration update: can be sent in both ways, to be refined
*/

void e1apCUUP_send_CONFIGURATION_UPDATE(sctp_assoc_t assoc_id)
{
  E1AP_E1AP_PDU_t pdu = {0};
  fill_CONFIGURATION_UPDATE(&pdu);
  e1ap_encode_send(UPtype, assoc_id, &pdu, 0, __func__);
}

int e1apCUCP_send_gNB_DU_CONFIGURATION_FAILURE(void)
{
  AssertFatal(false, "Not implemented yet\n");
  return -1;
}

int e1apCUCP_send_gNB_DU_CONFIGURATION_UPDATE_ACKNOWLEDGE(void)
{
  AssertFatal(false, "Not implemented yet\n");
  return -1;
}

int e1apCUCP_handle_CONFIGURATION_UPDATE(E1AP_E1AP_PDU_t *pdu)
{
  /*
  E1AP_GNB_CU_UP_E1SetupRequestIEs_t *ie;
  DevAssert(pdu != NULL);
  E1AP_GNB_CU_UP_E1SetupRequest_t *in = &pdu->choice.initiatingMessage->value.choice.GNB_CU_UP_E1SetupRequest;
  */

  AssertFatal(false, "Not implemented yet\n");
  return -1;
}

int e1apCUUP_handle_gNB_DU_CONFIGURATION_UPDATE_ACKNOWLEDGE(sctp_assoc_t assoc_id, uint32_t stream, E1AP_E1AP_PDU_t *pdu)
{
  AssertFatal(false, "Not implemented yet\n");
  return -1;
}

int e1apCUUP_handle_gNB_DU_CONFIGURATION_FAILURE(sctp_assoc_t assoc_id, uint32_t stream, E1AP_E1AP_PDU_t *pdu)
{
  AssertFatal(false, "Not implemented yet\n");
  return -1;
}

/*
  E1 release
*/

int e1ap_send_RELEASE_REQUEST(instance_t instance)
{
  AssertFatal(false, "Not implemented yet\n");
  return -1;
}

int e1ap_send_RELEASE_ACKNOWLEDGE(instance_t instance)
{
  AssertFatal(false, "Not implemented yet\n");
  return -1;
}

int e1ap_handle_RELEASE_REQUEST(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, E1AP_E1AP_PDU_t *pdu)
{
  AssertFatal(false, "Not implemented yet\n");
  return -1;
}

int e1ap_handle_RELEASE_ACKNOWLEDGE(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, E1AP_E1AP_PDU_t *pdu)
{
  AssertFatal(false, "Not implemented yet\n");
  return -1;
}

void e1apCUCP_send_BEARER_CONTEXT_SETUP_REQUEST(sctp_assoc_t assoc_id, e1ap_bearer_setup_req_t *const bearerCxt)
{
  E1AP_E1AP_PDU_t *pdu = encode_E1_bearer_context_setup_request(bearerCxt);
  e1ap_encode_send(CPtype, assoc_id, pdu, 0, __func__);
}

void e1apCUUP_send_BEARER_CONTEXT_SETUP_RESPONSE(sctp_assoc_t assoc_id, const e1ap_bearer_setup_resp_t *resp)
{
  // Encode
  E1AP_E1AP_PDU_t *pdu = encode_E1_bearer_context_setup_response(resp);
  e1ap_encode_send(UPtype, assoc_id, pdu, 0, __func__);
}

int e1apCUUP_send_BEARER_CONTEXT_SETUP_FAILURE(instance_t instance)
{
  AssertFatal(false, "Not implemented yet\n");
  return -1;
}

int e1apCUUP_handle_BEARER_CONTEXT_SETUP_REQUEST(sctp_assoc_t assoc_id, e1ap_upcp_inst_t *e1_inst, const E1AP_E1AP_PDU_t *pdu)
{
  // Decode E1AP message
  e1ap_bearer_setup_req_t bearerCxt = {0};
  if (!decode_E1_bearer_context_setup_request(pdu, &bearerCxt)) {
    LOG_E(E1AP, "Failed to decode E1 Bearer Context Setup Request\n");
    return -1;
  }
  e1_bearer_context_setup(&bearerCxt);
  return 0;
}

int e1apCUCP_handle_BEARER_CONTEXT_SETUP_RESPONSE(sctp_assoc_t assoc_id, e1ap_upcp_inst_t *inst, const E1AP_E1AP_PDU_t *pdu)
{
  MessageDef *msg = itti_alloc_new_message(TASK_CUCP_E1, 0, E1AP_BEARER_CONTEXT_SETUP_RESP);
  // Decode
  e1ap_bearer_setup_resp_t *bearerCxt = &E1AP_BEARER_CONTEXT_SETUP_RESP(msg);
  if (!decode_E1_bearer_context_setup_response(pdu, bearerCxt)) {
    LOG_E(E1AP, "Failed to decode Bearer Context Setup Response\n");
    return -1;
  }
  msg->ittiMsgHeader.originInstance = assoc_id;
  // Fixme: instance is the NGAP instance, no good way to set it here
  instance_t instance = 0;
  itti_send_msg_to_task(TASK_RRC_GNB, instance, msg);

  return 0;
}

int e1apCUCP_handle_BEARER_CONTEXT_SETUP_FAILURE(sctp_assoc_t assoc_id, e1ap_upcp_inst_t *inst, const E1AP_E1AP_PDU_t *pdu)
{
  AssertFatal(false, "Not implemented yet\n");
  return -1;
}

/*
  BEARER CONTEXT MODIFICATION REQUEST
*/

static int fill_BEARER_CONTEXT_MODIFICATION_REQUEST(e1ap_bearer_setup_req_t *const bearerCxt, E1AP_E1AP_PDU_t *pdu)
{
  pdu->present = E1AP_E1AP_PDU_PR_initiatingMessage;
  asn1cCalloc(pdu->choice.initiatingMessage, msg);
  msg->procedureCode = E1AP_ProcedureCode_id_bearerContextModification;
  msg->criticality   = E1AP_Criticality_reject;
  msg->value.present = E1AP_InitiatingMessage__value_PR_BearerContextModificationRequest;
  E1AP_BearerContextModificationRequest_t *out = &pdu->choice.initiatingMessage->value.choice.BearerContextModificationRequest;
  /* mandatory */
  /* c1. gNB-CU-CP UE E1AP ID */
  asn1cSequenceAdd(out->protocolIEs.list, E1AP_BearerContextModificationRequestIEs_t, ieC1);
  ieC1->id                         = E1AP_ProtocolIE_ID_id_gNB_CU_CP_UE_E1AP_ID;
  ieC1->criticality                = E1AP_Criticality_reject;
  ieC1->value.present              = E1AP_BearerContextModificationRequestIEs__value_PR_GNB_CU_CP_UE_E1AP_ID;
  ieC1->value.choice.GNB_CU_CP_UE_E1AP_ID = bearerCxt->gNB_cu_cp_ue_id;
  /* mandatory */
  /* c2. gNB-CU-UP UE E1AP ID */
  asn1cSequenceAdd(out->protocolIEs.list, E1AP_BearerContextModificationRequestIEs_t, ieC2);
  ieC2->id                         = E1AP_ProtocolIE_ID_id_gNB_CU_UP_UE_E1AP_ID;
  ieC2->criticality                = E1AP_Criticality_reject;
  ieC2->value.present              = E1AP_BearerContextModificationRequestIEs__value_PR_GNB_CU_UP_UE_E1AP_ID;
  ieC2->value.choice.GNB_CU_UP_UE_E1AP_ID = bearerCxt->gNB_cu_up_ue_id;
  /* optional */
  /* c3. E1AP_ProtocolIE_ID_id_System_BearerContextModificationRequest */
  asn1cSequenceAdd(out->protocolIEs.list, E1AP_BearerContextModificationRequestIEs_t, ieC3);
  ieC3->id            = E1AP_ProtocolIE_ID_id_System_BearerContextModificationRequest;
  ieC3->criticality   = E1AP_Criticality_reject;
  ieC3->value.present = E1AP_BearerContextModificationRequestIEs__value_PR_System_BearerContextModificationRequest;
  ieC3->value.choice.System_BearerContextModificationRequest.present = E1AP_System_BearerContextModificationRequest_PR_nG_RAN_BearerContextModificationRequest;
  E1AP_ProtocolIE_Container_4932P26_t *msgNGRAN_list = calloc(1, sizeof(E1AP_ProtocolIE_Container_4932P26_t));
  ieC3->value.choice.System_BearerContextModificationRequest.choice.nG_RAN_BearerContextModificationRequest = (struct E1AP_ProtocolIE_Container *) msgNGRAN_list;
  asn1cSequenceAdd(msgNGRAN_list->list, E1AP_NG_RAN_BearerContextModificationRequest_t, msgNGRAN);
  msgNGRAN->id = E1AP_ProtocolIE_ID_id_PDU_Session_Resource_To_Modify_List;
  msgNGRAN->criticality = E1AP_Criticality_reject;
  msgNGRAN->value.present = E1AP_NG_RAN_BearerContextModificationRequest__value_PR_PDU_Session_Resource_To_Modify_List;
  E1AP_PDU_Session_Resource_To_Modify_List_t *pdu2Setup = &msgNGRAN->value.choice.PDU_Session_Resource_To_Modify_List;
  for (pdu_session_to_mod_t *i = bearerCxt->pduSessionMod; i < bearerCxt->pduSessionMod + bearerCxt->numPDUSessionsMod; i++) {
    asn1cSequenceAdd(pdu2Setup->list, E1AP_PDU_Session_Resource_To_Modify_Item_t, ieC3_1);
    ieC3_1->pDU_Session_ID = i->sessionId;
    for (DRB_nGRAN_to_mod_t *j = i->DRBnGRanModList; j < i->DRBnGRanModList + i->numDRB2Modify; j++) {
      asn1cCalloc(ieC3_1->dRB_To_Modify_List_NG_RAN, drb2Mod_List);
      asn1cSequenceAdd(drb2Mod_List->list, E1AP_DRB_To_Modify_Item_NG_RAN_t, drb2Mod);
      drb2Mod->dRB_ID = j->id;
      asn1cCalloc(drb2Mod->pDCP_Configuration, pDCP_Configuration);
      if (j->pdcp_config.pDCP_Reestablishment) {
        asn1cCallocOne(pDCP_Configuration->pDCP_Reestablishment, E1AP_PDCP_Reestablishment_true);
      }
      if (j->numDlUpParam > 0) {
        asn1cCalloc(drb2Mod->dL_UP_Parameters, DL_UP_Param_List);
        for (up_params_t *k = j->DlUpParamList; k < j->DlUpParamList + j->numDlUpParam; k++) {
          asn1cSequenceAdd(DL_UP_Param_List->list, E1AP_UP_Parameters_Item_t, DL_UP_Param);
          DL_UP_Param->uP_TNL_Information.present = E1AP_UP_TNL_Information_PR_gTPTunnel;
          asn1cCalloc(DL_UP_Param->uP_TNL_Information.choice.gTPTunnel, gTPTunnel);
          TRANSPORT_LAYER_ADDRESS_IPv4_TO_BIT_STRING(k->tlAddress, &gTPTunnel->transportLayerAddress);
          INT32_TO_OCTET_STRING(k->teId, &gTPTunnel->gTP_TEID);

          DL_UP_Param->cell_Group_ID = k->cell_group_id;
        }
      }
    }
  }
  /* c4. E1AP_ProtocolIE_ID_id_BearerContextStatusChange */
  if (bearerCxt->bearerContextStatus == BEARER_SUSPEND) {
    asn1cSequenceAdd(out->protocolIEs.list, E1AP_BearerContextModificationRequestIEs_t, ieC4);
    ieC4->id            = E1AP_ProtocolIE_ID_id_BearerContextStatusChange;
    ieC4->criticality   = E1AP_Criticality_reject;
    ieC4->value.present = E1AP_BearerContextModificationRequestIEs__value_PR_BearerContextStatusChange;
    /* Bearer Context Status Change */
    ieC4->value.choice.BearerContextStatusChange = E1AP_BearerContextStatusChange_suspend;
  }
  return 0;
}

static void e1apCUCP_send_BEARER_CONTEXT_MODIFICATION_REQUEST(sctp_assoc_t assoc_id, e1ap_bearer_mod_req_t *const bearerCxt)
{
  E1AP_E1AP_PDU_t pdu = {0};
  fill_BEARER_CONTEXT_MODIFICATION_REQUEST(bearerCxt, &pdu);
  e1ap_encode_send(CPtype, assoc_id, &pdu, 0, __func__);
}

static void fill_BEARER_CONTEXT_MODIFICATION_RESPONSE(const e1ap_bearer_modif_resp_t *resp, E1AP_E1AP_PDU_t *pdu)
{
  pdu->present = E1AP_E1AP_PDU_PR_successfulOutcome;
  asn1cCalloc(pdu->choice.successfulOutcome, msg);
  msg->procedureCode = E1AP_ProcedureCode_id_bearerContextModification;
  msg->criticality = E1AP_Criticality_reject;
  msg->value.present = E1AP_SuccessfulOutcome__value_PR_BearerContextModificationResponse;
  E1AP_BearerContextModificationResponse_t *out = &pdu->choice.successfulOutcome->value.choice.BearerContextModificationResponse;

  /* mandatory */
  /* c1. gNB-CU-CP UE E1AP ID */
  asn1cSequenceAdd(out->protocolIEs.list, E1AP_BearerContextModificationResponseIEs_t, ieCuCpId);
  ieCuCpId->id = E1AP_ProtocolIE_ID_id_gNB_CU_CP_UE_E1AP_ID;
  ieCuCpId->criticality = E1AP_Criticality_reject;
  ieCuCpId->value.present = E1AP_BearerContextModificationResponseIEs__value_PR_GNB_CU_CP_UE_E1AP_ID;
  ieCuCpId->value.choice.GNB_CU_CP_UE_E1AP_ID = resp->gNB_cu_cp_ue_id;

  /* mandatory */
  /* c2. gNB-CU-UP UE E1AP ID */
  asn1cSequenceAdd(out->protocolIEs.list, E1AP_BearerContextModificationResponseIEs_t, ieCuUpId);
  ieCuUpId->id = E1AP_ProtocolIE_ID_id_gNB_CU_UP_UE_E1AP_ID;
  ieCuUpId->criticality = E1AP_Criticality_reject;
  ieCuUpId->value.present = E1AP_BearerContextModificationResponseIEs__value_PR_GNB_CU_UP_UE_E1AP_ID;
  ieCuUpId->value.choice.GNB_CU_UP_UE_E1AP_ID = resp->gNB_cu_up_ue_id;

  /* optional */
  /* PDU sessions that have been modified */
  asn1cSequenceAdd(out->protocolIEs.list, E1AP_BearerContextModificationResponseIEs_t, ieSys);
  ieSys->id = E1AP_ProtocolIE_ID_id_System_BearerContextModificationResponse;
  ieSys->criticality = E1AP_Criticality_ignore;
  ieSys->value.present = E1AP_BearerContextModificationResponseIEs__value_PR_System_BearerContextModificationResponse;

  E1AP_System_BearerContextModificationResponse_t *sys = &ieSys->value.choice.System_BearerContextModificationResponse;
  E1AP_ProtocolIE_Container_4932P29_t *piel = calloc(1, sizeof(*piel));
  sys->present = E1AP_System_BearerContextModificationResponse_PR_nG_RAN_BearerContextModificationResponse;
  sys->choice.nG_RAN_BearerContextModificationResponse = (struct E1AP_ProtocolIE_Container *)piel;
  asn1cSequenceAdd(piel->list, E1AP_NG_RAN_BearerContextModificationResponse_t, ngran_bcmr);
  ngran_bcmr->id = E1AP_ProtocolIE_ID_id_PDU_Session_Resource_Modified_List;
  ngran_bcmr->criticality = E1AP_Criticality_reject;
  ngran_bcmr->value.present = E1AP_NG_RAN_BearerContextModificationResponse__value_PR_PDU_Session_Resource_Modified_List;
  E1AP_PDU_Session_Resource_Modified_List_t *pdu_mod_l = &ngran_bcmr->value.choice.PDU_Session_Resource_Modified_List;
  for (int i = 0; i < resp->numPDUSessionsMod; ++i) {
    const pdu_session_modif_t *pdu = &resp->pduSessionMod[i];
    asn1cSequenceAdd(pdu_mod_l->list, E1AP_PDU_Session_Resource_Modified_Item_t, iePdu);
    iePdu->pDU_Session_ID = pdu->id;
    if (pdu->numDRBModified > 0) {
      iePdu->dRB_Modified_List_NG_RAN = calloc(1, sizeof(*iePdu->dRB_Modified_List_NG_RAN));
      AssertFatal(iePdu->dRB_Modified_List_NG_RAN != NULL, "out of memory\n");
      E1AP_DRB_Modified_List_NG_RAN_t *drb_mod_l = iePdu->dRB_Modified_List_NG_RAN;
      for (int j = 0; j < pdu->numDRBModified; ++j) {
        const DRB_nGRAN_modified_t *drb = &pdu->DRBnGRanModList[j];
        asn1cSequenceAdd(drb_mod_l->list, E1AP_DRB_Modified_Item_NG_RAN_t, drb_mod);
        drb_mod->dRB_ID = drb->id;
      }
    }
  }
}

static int e1apCUUP_send_BEARER_CONTEXT_MODIFICATION_RESPONSE(sctp_assoc_t assoc_id, const e1ap_bearer_modif_resp_t *resp)
{
  E1AP_E1AP_PDU_t pdu = {0};
  fill_BEARER_CONTEXT_MODIFICATION_RESPONSE(resp, &pdu);
  return e1ap_encode_send(UPtype, assoc_id, &pdu, 0, __func__);
}

int e1apCUUP_send_BEARER_CONTEXT_MODIFICATION_FAILURE(instance_t instance)
{
  AssertFatal(false, "Not implemented yet\n");
  return -1;
}

/**
 * @brief Process E1AP PDU and extract Bearer Context Modification Request
 */
static void extract_BEARER_CONTEXT_MODIFICATION_REQUEST(const E1AP_E1AP_PDU_t *pdu, e1ap_bearer_mod_req_t *bearerCxt)
{
  const E1AP_BearerContextModificationRequest_t *in = &pdu->choice.initiatingMessage->value.choice.BearerContextModificationRequest;
  E1AP_BearerContextModificationRequestIEs_t *ie;

  LOG_D(E1AP, "Bearer context setup number of IEs %d\n", in->protocolIEs.list.count);

  for (int i = 0; i < in->protocolIEs.list.count; i++) {
    ie = in->protocolIEs.list.array[i];

    switch(ie->id) {
      case E1AP_ProtocolIE_ID_id_gNB_CU_CP_UE_E1AP_ID:
        DevAssert(ie->criticality == E1AP_Criticality_reject);
        DevAssert(ie->value.present == E1AP_BearerContextModificationRequestIEs__value_PR_GNB_CU_CP_UE_E1AP_ID);
        bearerCxt->gNB_cu_cp_ue_id = ie->value.choice.GNB_CU_CP_UE_E1AP_ID;
        break;

      case E1AP_ProtocolIE_ID_id_gNB_CU_UP_UE_E1AP_ID:
        DevAssert(ie->criticality == E1AP_Criticality_reject);
        DevAssert(ie->value.present == E1AP_BearerContextModificationRequestIEs__value_PR_GNB_CU_UP_UE_E1AP_ID);
        bearerCxt->gNB_cu_up_ue_id = ie->value.choice.GNB_CU_UP_UE_E1AP_ID;
        break;

      case E1AP_ProtocolIE_ID_id_System_BearerContextModificationRequest:
        DevAssert(ie->criticality == E1AP_Criticality_reject);
        DevAssert(ie->value.present == E1AP_BearerContextModificationRequestIEs__value_PR_System_BearerContextModificationRequest);
        DevAssert(ie->value.choice.System_BearerContextModificationRequest.present ==
                    E1AP_System_BearerContextModificationRequest_PR_nG_RAN_BearerContextModificationRequest);
        DevAssert(ie->value.choice.System_BearerContextModificationRequest.choice.nG_RAN_BearerContextModificationRequest != NULL);
        E1AP_ProtocolIE_Container_4932P26_t *msgNGRAN_list = (E1AP_ProtocolIE_Container_4932P26_t *) ie->value.choice.System_BearerContextModificationRequest.choice.nG_RAN_BearerContextModificationRequest;
        E1AP_NG_RAN_BearerContextModificationRequest_t *msgNGRAN = msgNGRAN_list->list.array[0];
        DevAssert(msgNGRAN_list->list.count == 1);
        DevAssert(msgNGRAN->id == E1AP_ProtocolIE_ID_id_PDU_Session_Resource_To_Modify_List);
        DevAssert(msgNGRAN->value.present =
                      E1AP_NG_RAN_BearerContextModificationRequest__value_PR_PDU_Session_Resource_To_Modify_List);
        /* PDU Session Resource To Modify List (see 9.3.3.11 of TS 38.463) */
        E1AP_PDU_Session_Resource_To_Modify_List_t *pdu2ModList = &msgNGRAN->value.choice.PDU_Session_Resource_To_Modify_List;
        bearerCxt->numPDUSessionsMod = pdu2ModList->list.count;
        for (int i = 0; i < pdu2ModList->list.count; i++) {
          pdu_session_to_mod_t *pdu_session = bearerCxt->pduSessionMod + i;
          E1AP_PDU_Session_Resource_To_Modify_Item_t *pdu2Mod = pdu2ModList->list.array[i];
          pdu_session->sessionId = pdu2Mod->pDU_Session_ID;
          E1AP_DRB_To_Modify_List_NG_RAN_t *drb2ModList = pdu2Mod->dRB_To_Modify_List_NG_RAN;
          pdu_session->numDRB2Modify = drb2ModList->list.count;
          /* DRBs to modify */
          for (int j = 0; j < drb2ModList->list.count; j++) {
            DRB_nGRAN_to_mod_t *drb = pdu_session->DRBnGRanModList + j;
            E1AP_DRB_To_Modify_Item_NG_RAN_t *drb2Mod = drb2ModList->list.array[j];
            drb->id = drb2Mod->dRB_ID;
            if (drb2Mod->pDCP_Configuration != NULL
                && drb2Mod->pDCP_Configuration->pDCP_Reestablishment != NULL
                && *drb2Mod->pDCP_Configuration->pDCP_Reestablishment == E1AP_PDCP_Reestablishment_true) {
              drb->pdcp_config.pDCP_Reestablishment = true;
            }
            if (drb2Mod->dL_UP_Parameters) { /* Optional IE in the DRB To Modify Item */
              E1AP_UP_Parameters_t *dl_up_paramList = drb2Mod->dL_UP_Parameters;
              drb->numDlUpParam = dl_up_paramList->list.count;
              for (int k = 0; k < dl_up_paramList->list.count; k++) {
                up_params_t *dl_up_param = drb->DlUpParamList + k;
                E1AP_UP_Parameters_Item_t *dl_up_param_in = dl_up_paramList->list.array[k];
                if (dl_up_param_in->uP_TNL_Information.choice.gTPTunnel) { // Optional IE
                  DevAssert(dl_up_param_in->uP_TNL_Information.present = E1AP_UP_TNL_Information_PR_gTPTunnel);
                  BIT_STRING_TO_TRANSPORT_LAYER_ADDRESS_IPv4(
                      &dl_up_param_in->uP_TNL_Information.choice.gTPTunnel->transportLayerAddress,
                      dl_up_param->tlAddress);
                  OCTET_STRING_TO_INT32(&dl_up_param_in->uP_TNL_Information.choice.gTPTunnel->gTP_TEID, dl_up_param->teId);
                } else {
                  AssertFatal(false, "gTPTunnel IE is missing. It is mandatory at this point\n");
                }
                dl_up_param->cell_group_id = dl_up_param_in->cell_Group_ID;
              }
            }
          }
        }
        break;

      case E1AP_ProtocolIE_ID_id_BearerContextStatusChange:
        /* Bearer Context Status Change */
        DevAssert(ie->criticality == E1AP_Criticality_reject);
        DevAssert(ie->value.present == E1AP_BearerContextModificationRequestIEs__value_PR_BearerContextStatusChange);
        bearerCxt->bearerContextStatus = (ie->value.choice.BearerContextStatusChange == E1AP_BearerContextStatusChange_suspend) ? BEARER_SUSPEND : BEARER_ACTIVE;
        break;

      default:
        LOG_E(E1AP, "Handle for this IE is not implemented (or) invalid IE detected\n");
        break;
    }
  }
}

/**
 * @brief Extract Bearer Context Modification Request from E1AP PDU
 *        and prepare Bearer Context Modification Response
 */
int e1apCUUP_handle_BEARER_CONTEXT_MODIFICATION_REQUEST(sctp_assoc_t assoc_id,
                                                        e1ap_upcp_inst_t *e1_inst,
                                                        const E1AP_E1AP_PDU_t *pdu)
{
  DevAssert(pdu != NULL);
  DevAssert(pdu->present == E1AP_E1AP_PDU_PR_initiatingMessage);
  DevAssert(pdu->choice.initiatingMessage->procedureCode == E1AP_ProcedureCode_id_bearerContextModification);
  DevAssert(pdu->choice.initiatingMessage->criticality == E1AP_Criticality_reject);
  DevAssert(pdu->choice.initiatingMessage->value.present == E1AP_InitiatingMessage__value_PR_BearerContextModificationRequest);
  e1ap_bearer_mod_req_t bearerCxt = {0};
  extract_BEARER_CONTEXT_MODIFICATION_REQUEST(pdu, &bearerCxt);
  e1_bearer_context_modif(&bearerCxt);
  return 0;
}

static void extract_BEARER_CONTEXT_MODIFICATION_RESPONSE(const E1AP_E1AP_PDU_t *pdu, e1ap_bearer_modif_resp_t *resp)
{
  memset(resp, 0, sizeof(*resp));
  const E1AP_BearerContextModificationResponse_t *in =
      &pdu->choice.successfulOutcome->value.choice.BearerContextModificationResponse;

  for (int i = 0; i < in->protocolIEs.list.count; ++i) {
    E1AP_BearerContextModificationResponseIEs_t *ie = in->protocolIEs.list.array[i];

    switch (ie->id) {
      case E1AP_ProtocolIE_ID_id_gNB_CU_CP_UE_E1AP_ID:
        DevAssert(ie->criticality == E1AP_Criticality_reject);
        DevAssert(ie->value.present == E1AP_BearerContextModificationResponseIEs__value_PR_GNB_CU_CP_UE_E1AP_ID);
        resp->gNB_cu_cp_ue_id = ie->value.choice.GNB_CU_CP_UE_E1AP_ID;
        break;

      case E1AP_ProtocolIE_ID_id_gNB_CU_UP_UE_E1AP_ID:
        DevAssert(ie->criticality == E1AP_Criticality_reject);
        DevAssert(ie->value.present == E1AP_BearerContextModificationResponseIEs__value_PR_GNB_CU_UP_UE_E1AP_ID);
        resp->gNB_cu_up_ue_id = ie->value.choice.GNB_CU_UP_UE_E1AP_ID;
        break;

      case E1AP_ProtocolIE_ID_id_System_BearerContextModificationResponse:
        DevAssert(ie->criticality == E1AP_Criticality_ignore);
        DevAssert(ie->value.present
                  == E1AP_BearerContextModificationResponseIEs__value_PR_System_BearerContextModificationResponse);
        DevAssert(ie->value.choice.System_BearerContextModificationResponse.present
                  == E1AP_System_BearerContextModificationResponse_PR_nG_RAN_BearerContextModificationResponse);
        E1AP_ProtocolIE_Container_4932P29_t *msgNGRAN_list =
            (E1AP_ProtocolIE_Container_4932P29_t *)
                ie->value.choice.System_BearerContextModificationResponse.choice.nG_RAN_BearerContextModificationResponse;
        DevAssert(msgNGRAN_list->list.count == 1);
        E1AP_NG_RAN_BearerContextModificationResponse_t *msgNGRAN = msgNGRAN_list->list.array[0];
        DevAssert(msgNGRAN->id == E1AP_ProtocolIE_ID_id_PDU_Session_Resource_Modified_List);
        DevAssert(msgNGRAN->criticality == E1AP_Criticality_reject);
        DevAssert(msgNGRAN->value.present
                  == E1AP_NG_RAN_BearerContextModificationResponse__value_PR_PDU_Session_Resource_Modified_List);
        E1AP_PDU_Session_Resource_Modified_List_t *pduModifiedList = &msgNGRAN->value.choice.PDU_Session_Resource_Modified_List;
        resp->numPDUSessionsMod = pduModifiedList->list.count;

        for (int i = 0; i < pduModifiedList->list.count; i++) {
          pdu_session_modif_t *pduModified = &resp->pduSessionMod[i];
          E1AP_PDU_Session_Resource_Modified_Item_t *mod_it = pduModifiedList->list.array[i];
          pduModified->id = mod_it->pDU_Session_ID;

          AssertFatal(mod_it->nG_DL_UP_TNL_Information == NULL, "not implemented\n");
          AssertFatal(mod_it->securityResult == NULL, "not implemented\n");
          AssertFatal(mod_it->pDU_Session_Data_Forwarding_Information_Response == NULL, "not implemented\n");
          AssertFatal(mod_it->dRB_Setup_List_NG_RAN == NULL, "not implemented\n");
          AssertFatal(mod_it->dRB_Failed_List_NG_RAN == NULL, "not implemented\n");
          AssertFatal(mod_it->dRB_Failed_To_Modify_List_NG_RAN == NULL, "not implemented\n");

          pduModified->numDRBModified = mod_it->dRB_Modified_List_NG_RAN->list.count;
          for (int j = 0; j < mod_it->dRB_Modified_List_NG_RAN->list.count; j++) {
            DRB_nGRAN_modified_t *drbModified = &pduModified->DRBnGRanModList[j];
            E1AP_DRB_Modified_Item_NG_RAN_t *drb = mod_it->dRB_Modified_List_NG_RAN->list.array[j];

            drbModified->id = drb->dRB_ID;

            AssertFatal(drb->uL_UP_Transport_Parameters == NULL, "not implemented\n");
            AssertFatal(drb->pDCP_SN_Status_Information == NULL, "not implemented\n");
            AssertFatal(drb->flow_Setup_List == NULL, "not implemented\n");
            AssertFatal(drb->flow_Failed_List == NULL, "not implemented\n");
          }
        }
        break;
    }
  }
}

int e1apCUCP_handle_BEARER_CONTEXT_MODIFICATION_RESPONSE(sctp_assoc_t assoc_id,
                                                         e1ap_upcp_inst_t *e1_inst,
                                                         const E1AP_E1AP_PDU_t *pdu)
{
  DevAssert(pdu != NULL);
  DevAssert(pdu->present == E1AP_E1AP_PDU_PR_successfulOutcome);
  DevAssert(pdu->choice.successfulOutcome->procedureCode == E1AP_ProcedureCode_id_bearerContextModification);
  DevAssert(pdu->choice.successfulOutcome->criticality == E1AP_Criticality_reject);
  DevAssert(pdu->choice.successfulOutcome->value.present == E1AP_SuccessfulOutcome__value_PR_BearerContextModificationResponse);
  MessageDef *msg = itti_alloc_new_message(TASK_CUUP_E1, 0, E1AP_BEARER_CONTEXT_MODIFICATION_RESP);
  e1ap_bearer_modif_resp_t *modif = &E1AP_BEARER_CONTEXT_MODIFICATION_RESP(msg);
  extract_BEARER_CONTEXT_MODIFICATION_RESPONSE(pdu, modif);
  itti_send_msg_to_task(TASK_RRC_GNB, 0, msg);
  return 0;
}

int e1apCUCP_handle_BEARER_CONTEXT_MODIFICATION_FAILURE(instance_t instance,
                                                        sctp_assoc_t assoc_id,
                                                        uint32_t stream,
                                                        E1AP_E1AP_PDU_t *pdu)
{
  AssertFatal(false, "Not implemented yet\n");
  return -1;
}

int e1apCUUP_send_BEARER_CONTEXT_MODIFICATION_REQUIRED(instance_t instance)
{
  AssertFatal(false, "Not implemented yet\n");
  return -1;
}

int e1apCUCP_send_BEARER_CONTEXT_MODIFICATION_CONFIRM(instance_t instance)
{
  AssertFatal(false, "Not implemented yet\n");
  return -1;
}

int e1apCUCP_handle_BEARER_CONTEXT_MODIFICATION_REQUIRED(instance_t instance,
                                                         sctp_assoc_t assoc_id,
                                                         uint32_t stream,
                                                         E1AP_E1AP_PDU_t *pdu)
{
  AssertFatal(false, "Not implemented yet\n");
  return -1;
}

int e1apCUUP_handle_BEARER_CONTEXT_MODIFICATION_CONFIRM(instance_t instance,
                                                        sctp_assoc_t assoc_id,
                                                        uint32_t stream,
                                                        E1AP_E1AP_PDU_t *pdu)
{
  AssertFatal(false, "Not implemented yet\n");
  return -1;
}
/*
  BEARER CONTEXT RELEASE
*/

static int fill_BEARER_CONTEXT_RELEASE_COMMAND(e1ap_bearer_release_cmd_t *const cmd, E1AP_E1AP_PDU_t *pdu)
{
  pdu->present = E1AP_E1AP_PDU_PR_initiatingMessage;
  asn1cCalloc(pdu->choice.initiatingMessage, msg);
  msg->procedureCode = E1AP_ProcedureCode_id_bearerContextRelease;
  msg->criticality   = E1AP_Criticality_reject;
  msg->value.present = E1AP_InitiatingMessage__value_PR_BearerContextReleaseCommand;
  E1AP_BearerContextReleaseCommand_t *out = &pdu->choice.initiatingMessage->value.choice.BearerContextReleaseCommand;
  /* mandatory */
  /* c1. gNB-CU-CP UE E1AP ID */
  asn1cSequenceAdd(out->protocolIEs.list, E1AP_BearerContextReleaseCommandIEs_t, ieC1);
  ieC1->id                         = E1AP_ProtocolIE_ID_id_gNB_CU_CP_UE_E1AP_ID;
  ieC1->criticality                = E1AP_Criticality_reject;
  ieC1->value.present              = E1AP_BearerContextReleaseCommandIEs__value_PR_GNB_CU_CP_UE_E1AP_ID;
  ieC1->value.choice.GNB_CU_CP_UE_E1AP_ID = cmd->gNB_cu_cp_ue_id;
  /* mandatory */
  /* c2. gNB-CU-UP UE E1AP ID */
  asn1cSequenceAdd(out->protocolIEs.list, E1AP_BearerContextReleaseCommandIEs_t, ieC2);
  ieC2->id                         = E1AP_ProtocolIE_ID_id_gNB_CU_UP_UE_E1AP_ID;
  ieC2->criticality                = E1AP_Criticality_reject;
  ieC2->value.present              = E1AP_BearerContextReleaseCommandIEs__value_PR_GNB_CU_UP_UE_E1AP_ID;
  ieC2->value.choice.GNB_CU_UP_UE_E1AP_ID = cmd->gNB_cu_cp_ue_id;

  return 0;
}

int e1apCUCP_send_BEARER_CONTEXT_RELEASE_COMMAND(sctp_assoc_t assoc_id, e1ap_bearer_release_cmd_t *const cmd)
{
  E1AP_E1AP_PDU_t pdu = {0};
  fill_BEARER_CONTEXT_RELEASE_COMMAND(cmd, &pdu);
  return e1ap_encode_send(CPtype, assoc_id, &pdu, 0, __func__);
}

static int fill_BEARER_CONTEXT_RELEASE_COMPLETE(const e1ap_bearer_release_cplt_t *cplt, E1AP_E1AP_PDU_t *pdu)
{
  pdu->present = E1AP_E1AP_PDU_PR_successfulOutcome;
  asn1cCalloc(pdu->choice.successfulOutcome, msg);
  msg->procedureCode = E1AP_ProcedureCode_id_bearerContextRelease;
  msg->criticality   = E1AP_Criticality_reject;
  msg->value.present = E1AP_SuccessfulOutcome__value_PR_BearerContextReleaseComplete;
  E1AP_BearerContextReleaseComplete_t *out = &pdu->choice.successfulOutcome->value.choice.BearerContextReleaseComplete;
  /* mandatory */
  /* c1. gNB-CU-CP UE E1AP ID */
  asn1cSequenceAdd(out->protocolIEs.list, E1AP_BearerContextReleaseCompleteIEs_t, ieC1);
  ieC1->id                         = E1AP_ProtocolIE_ID_id_gNB_CU_CP_UE_E1AP_ID;
  ieC1->criticality                = E1AP_Criticality_reject;
  ieC1->value.present              = E1AP_BearerContextReleaseCompleteIEs__value_PR_GNB_CU_CP_UE_E1AP_ID;
  ieC1->value.choice.GNB_CU_CP_UE_E1AP_ID = cplt->gNB_cu_cp_ue_id;
  /* mandatory */
  /* c2. gNB-CU-UP UE E1AP ID */
  asn1cSequenceAdd(out->protocolIEs.list, E1AP_BearerContextReleaseCompleteIEs_t, ieC2);
  ieC2->id                         = E1AP_ProtocolIE_ID_id_gNB_CU_UP_UE_E1AP_ID;
  ieC2->criticality                = E1AP_Criticality_reject;
  ieC2->value.present              = E1AP_BearerContextReleaseCompleteIEs__value_PR_GNB_CU_UP_UE_E1AP_ID;
  ieC2->value.choice.GNB_CU_UP_UE_E1AP_ID = cplt->gNB_cu_cp_ue_id;

  return 0;
}

int e1apCUUP_send_BEARER_CONTEXT_RELEASE_COMPLETE(sctp_assoc_t assoc_id, const e1ap_bearer_release_cplt_t *cplt)
{
  E1AP_E1AP_PDU_t pdu = {0};
  fill_BEARER_CONTEXT_RELEASE_COMPLETE(cplt, &pdu);
  return e1ap_encode_send(CPtype, assoc_id, &pdu, 0, __func__);
}

int e1apCUUP_send_BEARER_CONTEXT_RELEASE_REQUEST(instance_t instance)
{
  AssertFatal(false, "Not implemented yet\n");
  return -1;
}

void extract_BEARER_CONTEXT_RELEASE_COMMAND(const E1AP_E1AP_PDU_t *pdu, e1ap_bearer_release_cmd_t *bearerCxt)
{
  const E1AP_BearerContextReleaseCommand_t *in = &pdu->choice.initiatingMessage->value.choice.BearerContextReleaseCommand;
  E1AP_BearerContextReleaseCommandIEs_t *ie;

  LOG_D(E1AP, "Bearer context setup number of IEs %d\n", in->protocolIEs.list.count);

  for (int i = 0; i < in->protocolIEs.list.count; i++) {
    ie = in->protocolIEs.list.array[i];

    switch(ie->id) {
      case E1AP_ProtocolIE_ID_id_gNB_CU_CP_UE_E1AP_ID:
        DevAssert(ie->criticality == E1AP_Criticality_reject);
        DevAssert(ie->value.present == E1AP_BearerContextReleaseCommandIEs__value_PR_GNB_CU_CP_UE_E1AP_ID);
        bearerCxt->gNB_cu_cp_ue_id = ie->value.choice.GNB_CU_CP_UE_E1AP_ID;
        break;

      case E1AP_ProtocolIE_ID_id_gNB_CU_UP_UE_E1AP_ID:
        DevAssert(ie->criticality == E1AP_Criticality_reject);
        DevAssert(ie->value.present == E1AP_BearerContextReleaseCommandIEs__value_PR_GNB_CU_UP_UE_E1AP_ID);
        bearerCxt->gNB_cu_up_ue_id = ie->value.choice.GNB_CU_UP_UE_E1AP_ID;
        break;

      case E1AP_ProtocolIE_ID_id_Cause:
        DevAssert(ie->criticality == E1AP_Criticality_ignore);
        DevAssert(ie->value.present == E1AP_BearerContextReleaseCommandIEs__value_PR_Cause);
        bearerCxt->cause_type = ie->value.choice.Cause.present;
        if ((ie->value.choice.Cause.present != E1AP_Cause_PR_NOTHING) &&
            (ie->value.choice.Cause.present != E1AP_Cause_PR_choice_extension))
          bearerCxt->cause = ie->value.choice.Cause.choice.radioNetwork;
        break;
                                                 
      default:
        LOG_E(E1AP, "Handle for this IE is not implemented (or) invalid IE detected\n");
        break;
    }
  }
}

int e1apCUUP_handle_BEARER_CONTEXT_RELEASE_COMMAND(sctp_assoc_t assoc_id, e1ap_upcp_inst_t *e1_inst, const E1AP_E1AP_PDU_t *pdu)
{
  DevAssert(pdu != NULL);
  DevAssert(pdu->present == E1AP_E1AP_PDU_PR_initiatingMessage);
  DevAssert(pdu->choice.initiatingMessage->procedureCode == E1AP_ProcedureCode_id_bearerContextRelease);
  DevAssert(pdu->choice.initiatingMessage->criticality == E1AP_Criticality_reject);
  DevAssert(pdu->choice.initiatingMessage->value.present == E1AP_InitiatingMessage__value_PR_BearerContextReleaseCommand);

  e1ap_bearer_release_cmd_t bearerCxt = {0};
  extract_BEARER_CONTEXT_RELEASE_COMMAND(pdu, &bearerCxt);
  e1_bearer_release_cmd(&bearerCxt);
  return 0;
}

void extract_BEARER_CONTEXT_RELEASE_COMPLETE(const E1AP_E1AP_PDU_t *pdu, e1ap_bearer_release_cplt_t *bearerCxt)
{
  const E1AP_BearerContextReleaseComplete_t *in = &pdu->choice.successfulOutcome->value.choice.BearerContextReleaseComplete;
  E1AP_BearerContextReleaseCompleteIEs_t *ie;

  LOG_D(E1AP, "Bearer context setup number of IEs %d\n", in->protocolIEs.list.count);

  for (int i = 0; i < in->protocolIEs.list.count; i++) {
    ie = in->protocolIEs.list.array[i];

    switch(ie->id) {
      case E1AP_ProtocolIE_ID_id_gNB_CU_CP_UE_E1AP_ID:
        DevAssert(ie->criticality == E1AP_Criticality_reject);
        DevAssert(ie->value.present == E1AP_BearerContextReleaseCompleteIEs__value_PR_GNB_CU_CP_UE_E1AP_ID);
        bearerCxt->gNB_cu_cp_ue_id = ie->value.choice.GNB_CU_CP_UE_E1AP_ID;
        break;

      case E1AP_ProtocolIE_ID_id_gNB_CU_UP_UE_E1AP_ID:
        DevAssert(ie->criticality == E1AP_Criticality_reject);
        DevAssert(ie->value.present == E1AP_BearerContextReleaseCompleteIEs__value_PR_GNB_CU_UP_UE_E1AP_ID);
        bearerCxt->gNB_cu_up_ue_id = ie->value.choice.GNB_CU_UP_UE_E1AP_ID;
        break;

      default:
        LOG_E(E1AP, "Handle for this IE is not implemented (or) invalid IE detected\n");
        break;
    }
  }
}

int e1apCUCP_handle_BEARER_CONTEXT_RELEASE_COMPLETE(sctp_assoc_t assoc_id, e1ap_upcp_inst_t *e1_inst, const E1AP_E1AP_PDU_t *pdu)
{
  DevAssert(pdu != NULL);
  DevAssert(pdu->present == E1AP_E1AP_PDU_PR_successfulOutcome);
  DevAssert(pdu->choice.successfulOutcome->procedureCode == E1AP_ProcedureCode_id_bearerContextRelease);
  DevAssert(pdu->choice.successfulOutcome->criticality == E1AP_Criticality_reject);
  DevAssert(pdu->choice.successfulOutcome->value.present == E1AP_SuccessfulOutcome__value_PR_BearerContextReleaseComplete);

  e1ap_bearer_release_cplt_t bearerCxt = {0};
  extract_BEARER_CONTEXT_RELEASE_COMPLETE(pdu, &bearerCxt);
  MessageDef *msg = itti_alloc_new_message(TASK_CUUP_E1, 0, E1AP_BEARER_CONTEXT_RELEASE_CPLT);
  e1ap_bearer_release_cplt_t *cplt = &E1AP_BEARER_CONTEXT_RELEASE_CPLT(msg);
  *cplt = bearerCxt;
  itti_send_msg_to_task(TASK_RRC_GNB, 0, msg);
  return 0;
}

int e1apCUCP_handle_BEARER_CONTEXT_RELEASE_REQUEST(instance_t instance,
                                                   sctp_assoc_t assoc_id,
                                                   uint32_t stream,
                                                   E1AP_E1AP_PDU_t *pdu)
{
  AssertFatal(false, "Not implemented yet\n");
  return -1;
}

/*
BEARER CONTEXT INACTIVITY NOTIFICATION
 */

int e1apCUUP_send_BEARER_CONTEXT_INACTIVITY_NOTIFICATION(instance_t instance)
{
  AssertFatal(false, "Not implemented yet\n");
  return -1;
}

int e1apCUCP_handle_BEARER_CONTEXT_INACTIVITY_NOTIFICATION(instance_t instance,
                                                           sctp_assoc_t assoc_id,
                                                           uint32_t stream,
                                                           E1AP_E1AP_PDU_t *pdu)
{
  AssertFatal(false, "Not implemented yet\n");
  return -1;
}
/*
  DL DATA
*/

int e1apCUUP_send_DL_DATA_NOTIFICATION(instance_t instance)
{
  AssertFatal(false, "Not implemented yet\n");
  return -1;
}

int e1apCUUP_send_DATA_USAGE_REPORT(instance_t instance)
{
  AssertFatal(false, "Not implemented yet\n");
  return -1;
}

int e1apCUCP_handle_DL_DATA_NOTIFICATION(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, E1AP_E1AP_PDU_t *pdu)
{
  AssertFatal(false, "Not implemented yet\n");
  return -1;
}

int e1apCUCP_handle_send_DATA_USAGE_REPORT(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, E1AP_E1AP_PDU_t *pdu)
{
  AssertFatal(false, "Not implemented yet\n");
  return -1;
}

static instance_t cuup_task_create_gtpu_instance_to_du(eth_params_t *IPaddrs)
{
  openAddr_t tmp = {0};
  strncpy(tmp.originHost, IPaddrs->my_addr, sizeof(tmp.originHost) - 1);
  sprintf(tmp.originService, "%d", IPaddrs->my_portd);
  sprintf(tmp.destinationService, "%d", IPaddrs->remote_portd);
  return gtpv1Init(tmp);
}

static void e1_task_send_sctp_association_req(long task_id, instance_t instance, e1ap_net_config_t *e1ap_nc)
{
  DevAssert(e1ap_nc != NULL);
  getCxtE1(instance)->sockState = SCTP_STATE_CLOSED;
  MessageDef *message_p = itti_alloc_new_message(task_id, 0, SCTP_NEW_ASSOCIATION_REQ);
  sctp_new_association_req_t *sctp_new_req = &message_p->ittiMsg.sctp_new_association_req;
  sctp_new_req->ulp_cnx_id = instance;
  sctp_new_req->port = E1AP_PORT_NUMBER;
  sctp_new_req->ppid = E1AP_SCTP_PPID;
  // remote
  sctp_new_req->remote_address = e1ap_nc->CUCP_e1_ip_address;
  // local
  sctp_new_req->local_address = e1ap_nc->CUUP_e1_ip_address;
  itti_send_msg_to_task(TASK_SCTP, instance, message_p);
}

static void e1apCUUP_send_SETUP_REQUEST(sctp_assoc_t assoc_id, e1ap_setup_req_t *setup)
{
  setup->transac_id = E1AP_get_next_transaction_identifier();
  LOG_D(E1AP, "Transaction ID of setup request %ld\n", setup->transac_id);
  E1AP_E1AP_PDU_t *pdu = encode_e1ap_cuup_setup_request(setup);
  e1ap_encode_send(UPtype, assoc_id, pdu, 0, __func__);
}

/**
 * @brief SCTP association response handler (CU-CP to/from CU-UP)
 * it behaves differently depending on the type of E1 instance,
 * CUCP: informs RRC of E1 connection loss with CU-UP
 * CUUP: triggers a new SCTP association request by sending an ITTI to the CU-UP task
 * @param type indicates whether the handler is for the CU-CP or CU-UP
 */
static void e1_task_handle_sctp_association_resp(E1_t type,
                                                 instance_t instance,
                                                 sctp_new_association_resp_t *sctp_new_association_resp)
{
  DevAssert(sctp_new_association_resp != NULL);
  getCxtE1(instance)->sockState = sctp_new_association_resp->sctp_state;

  // Handle SCTP establishment failure
  if (sctp_new_association_resp->sctp_state != SCTP_STATE_ESTABLISHED) {
    if (type == CPtype) {
      // Inform RRC that the CU-UP is gone
      LOG_W(E1AP,
            "Lost connection (%s) with CU-UP (assoc_id %d)\n",
            sctp_state_s[sctp_new_association_resp->sctp_state],
            sctp_new_association_resp->assoc_id);
      MessageDef *message_p = itti_alloc_new_message(TASK_CUCP_E1, 0, E1AP_LOST_CONNECTION);
      message_p->ittiMsgHeader.originInstance = sctp_new_association_resp->assoc_id;
      itti_send_msg_to_task(TASK_RRC_GNB, instance, message_p);
    } else if (type == UPtype) {
      // Trigger new E1 association when no CU-CP is connected
      LOG_W(E1AP,
            "Lost connection (%s) with CU-CP (assoc_id %d): trigger new E1 association\n",
            sctp_state_s[sctp_new_association_resp->sctp_state],
            sctp_new_association_resp->assoc_id);
      long timer_id;
      timer_setup(1, 0, TASK_CUUP_E1, 0, TIMER_ONE_SHOT, NULL, &timer_id);
    }
    return;
  }

  if (type == UPtype) {
    e1ap_upcp_inst_t *inst = getCxtE1(instance);
    inst->cuup.assoc_id = sctp_new_association_resp->assoc_id;

    e1ap_net_config_t *nc = &inst->net_config;
    eth_params_t IPaddr = {0};
    IPaddr.my_addr = nc->localAddressF1U;
    IPaddr.my_portd = nc->localPortF1U;
    IPaddr.remote_portd = nc->remotePortF1U;
    if (getCxtE1(instance)->gtpInstF1U < 0)
      getCxtE1(instance)->gtpInstF1U = cuup_task_create_gtpu_instance_to_du(&IPaddr);
    if (getCxtE1(instance)->gtpInstF1U < 0)
      LOG_E(E1AP, "Failed to create CUUP F1-U UDP listener");
    extern instance_t CUuniqInstance;
    CUuniqInstance = getCxtE1(instance)->gtpInstF1U;
    cuup_init_n3(instance);
    e1apCUUP_send_SETUP_REQUEST(inst->cuup.assoc_id, &inst->cuup.setupReq);
  }
}

void cuup_init_n3(instance_t instance)
{
  if (getCxtE1(instance)->gtpInstN3 < 0) {
    e1ap_net_config_t *nc = &getCxtE1(instance)->net_config;
    openAddr_t tmp = {0};
    strcpy(tmp.originHost, nc->localAddressN3);
    sprintf(tmp.originService, "%d", nc->localPortN3);
    sprintf(tmp.destinationService, "%d", nc->remotePortN3);
    LOG_I(GTPU, "Configuring GTPu address : %s, port : %s\n", tmp.originHost, tmp.originService);
    // Fixme: fully inconsistent instances management
    // dirty global var is a bad fix
    extern instance_t legacyInstanceMapping;
    legacyInstanceMapping = getCxtE1(instance)->gtpInstN3 = gtpv1Init(tmp);
  }
  if (getCxtE1(instance)->gtpInstN3 < 0)
    LOG_E(E1AP, "Failed to create CUUP N3 UDP listener");
  extern instance_t *N3GTPUInst;
  N3GTPUInst = &getCxtE1(instance)->gtpInstN3;
}

void cucp_task_send_sctp_init_req(instance_t instance, char *my_addr)
{
  size_t addr_len = strlen(my_addr) + 1;
  LOG_I(E1AP, "E1AP_CUCP_SCTP_REQ(create socket) for %s len %ld\n", my_addr, addr_len);
  MessageDef *message_p = itti_alloc_new_message_sized(TASK_CUCP_E1, 0, SCTP_INIT_MSG, sizeof(sctp_init_t) + addr_len);
  sctp_init_t *init = &SCTP_INIT_MSG(message_p);
  init->port = E1AP_PORT_NUMBER;
  init->ppid = E1AP_SCTP_PPID;
  char *addr_buf = (char *) (init + 1); // address after sctp_init ITTI message end, allocated above
  init->bind_address = addr_buf;
  memcpy(addr_buf, my_addr, addr_len);
  itti_send_msg_to_task(TASK_SCTP, instance, message_p);
}

void e1_task_handle_sctp_association_ind(E1_t type, instance_t instance, sctp_new_association_ind_t *sctp_new_ind)
{
  if (!getCxtE1(instance))
    createE1inst(type, instance, 0, NULL, NULL);
  e1ap_upcp_inst_t *inst = getCxtE1(instance);
  inst->sockState = SCTP_STATE_ESTABLISHED;
  if (type == UPtype)
    inst->cuup.assoc_id = sctp_new_ind->assoc_id;
}

/**
 * @brief Handle the timer triggered by a connection loss with CU-CP
 *        it uses a stored network configuration and send a new SCTP association request
 */
static void e1apHandleTimer(instance_t myInstance)
{
  LOG_W(E1AP, "Try to reconnect to CP\n");
  if (getCxtE1(myInstance)->sockState != SCTP_STATE_ESTABLISHED)
    e1_task_send_sctp_association_req(TASK_CUUP_E1, myInstance, &getCxtE1(myInstance)->net_config);
}

/**
 * @brief E1AP Centralized Unit Control Plane (CU-CP) task function.
 *
 * This is the main task loop for the E1AP CUCP module:
 * it listens for incoming messages from the Inter-Task Interface (ITTI)
 * and calls the relevant handlers for each message type.
 *
 */
void *E1AP_CUCP_task(void *arg)
{
  LOG_I(E1AP, "Starting E1AP at CU CP\n");
  MessageDef *msg = NULL;
  e1ap_common_init();
  int result;

  while (1) {
    itti_receive_msg(TASK_CUCP_E1, &msg);
    instance_t myInstance = ITTI_MSG_DESTINATION_INSTANCE(msg);
    const int msgType = ITTI_MSG_ID(msg);
    LOG_D(E1AP, "CUCP received %s for instance %ld\n", messages_info[msgType].name, myInstance);
    sctp_assoc_t assoc_id = msg->ittiMsgHeader.originInstance;

    switch (ITTI_MSG_ID(msg)) {
      case SCTP_NEW_ASSOCIATION_IND:
        e1_task_handle_sctp_association_ind(CPtype, ITTI_MSG_ORIGIN_INSTANCE(msg), &msg->ittiMsg.sctp_new_association_ind);
        break;

      case SCTP_NEW_ASSOCIATION_RESP:
        e1_task_handle_sctp_association_resp(CPtype, ITTI_MSG_ORIGIN_INSTANCE(msg), &msg->ittiMsg.sctp_new_association_resp);
        break;

      case E1AP_REGISTER_REQ: {
        // note: we use E1AP_REGISTER_REQ only to set up sockets, the
        // E1AP_SETUP_REQ is not used and comes through the socket from the
        // CU-UP later!
        e1ap_net_config_t *nc = &E1AP_REGISTER_REQ(msg).net_config;
        AssertFatal(nc->CUCP_e1_ip_address.ipv4 != 0, "No IPv4 address configured\n");
        cucp_task_send_sctp_init_req(0, nc->CUCP_e1_ip_address.ipv4_address);
      } break;

      case E1AP_SETUP_REQ:
        AssertFatal(false, "should not receive E1AP_SETUP_REQ in CU-CP!\n");
        break;

      case SCTP_DATA_IND:
        e1_task_handle_sctp_data_ind(myInstance, &msg->ittiMsg.sctp_data_ind);
        break;

      case E1AP_SETUP_RESP:
        e1ap_send_SETUP_RESPONSE(assoc_id, &E1AP_SETUP_RESP(msg));
        free_e1ap_cuup_setup_response(&E1AP_SETUP_RESP(msg));
        break;

      case E1AP_SETUP_FAIL:
        e1apCUCP_send_SETUP_FAILURE(assoc_id, &E1AP_SETUP_FAIL(msg));
        free_e1ap_cuup_setup_failure(&E1AP_SETUP_FAIL(msg));
        break;

      case E1AP_BEARER_CONTEXT_SETUP_REQ:
        e1apCUCP_send_BEARER_CONTEXT_SETUP_REQUEST(assoc_id, &E1AP_BEARER_CONTEXT_SETUP_REQ(msg));
        free_e1ap_context_setup_request(&E1AP_BEARER_CONTEXT_SETUP_REQ(msg));
        break;

      case E1AP_BEARER_CONTEXT_MODIFICATION_REQ:
        e1apCUCP_send_BEARER_CONTEXT_MODIFICATION_REQUEST(assoc_id, &E1AP_BEARER_CONTEXT_MODIFICATION_REQ(msg));
        break;

      case E1AP_BEARER_CONTEXT_RELEASE_CMD:
        e1apCUCP_send_BEARER_CONTEXT_RELEASE_COMMAND(assoc_id, &E1AP_BEARER_CONTEXT_RELEASE_CMD(msg));
        break;

      default:
        LOG_E(E1AP, "Unknown message received in TASK_CUCP_E1\n");
        break;
    }

    result = itti_free(ITTI_MSG_ORIGIN_ID(msg), msg);
    AssertFatal(result == EXIT_SUCCESS, "Failed to free memory (%d) in E1AP_CUCP_task!\n", result);
    msg = NULL;
  }
}

/**
 * @brief E1AP Centralized Unit User Plane (CU-UP) task function.
 *
 * This is the main task loop for the E1AP CUUP module:
 * it listens for incoming messages from the Inter-Task Interface (ITTI)
 * and calls the relevant handlers for each message type.
 *
 */
void *E1AP_CUUP_task(void *arg)
{
  LOG_I(E1AP, "Starting E1AP at CU UP\n");
  e1ap_common_init();
  int result;

  // SCTP
  while (1) {
    MessageDef *msg = NULL;
    itti_receive_msg(TASK_CUUP_E1, &msg);
    const instance_t myInstance = ITTI_MSG_DESTINATION_INSTANCE(msg);
    const int msgType = ITTI_MSG_ID(msg);
    LOG_D(E1AP, "CUUP received %s for instance %ld\n", messages_info[msgType].name, myInstance);
    switch (msgType) {
      case E1AP_REGISTER_REQ: {
        /* E1AP Register Request triggered at startup
        create a new E1 instance and send the first association request */
        e1ap_register_req_t *reg_req = &E1AP_REGISTER_REQ(msg);
        createE1inst(UPtype, myInstance, reg_req->gnb_id, &reg_req->net_config, &reg_req->setup_req);
        e1_task_send_sctp_association_req(TASK_CUUP_E1, myInstance, &reg_req->net_config);
      } break;

      case SCTP_NEW_ASSOCIATION_RESP:
        e1_task_handle_sctp_association_resp(UPtype, myInstance, &msg->ittiMsg.sctp_new_association_resp);
        if (getCxtE1(myInstance)->sockState == SCTP_STATE_ESTABLISHED)
          LOG_A(E1AP, "E1 connection established (%s)\n", sctp_state_s[getCxtE1(myInstance)->sockState]);
        break;

      case SCTP_DATA_IND:
        e1_task_handle_sctp_data_ind(myInstance, &msg->ittiMsg.sctp_data_ind);
        break;

      case TIMER_HAS_EXPIRED:
        // Timer triggered by a connection loss with CU-CP
        e1apHandleTimer(myInstance);
        break;

      case E1AP_BEARER_CONTEXT_SETUP_RESP: {
        const e1ap_bearer_setup_resp_t *resp = &E1AP_BEARER_CONTEXT_SETUP_RESP(msg);
        const e1ap_upcp_inst_t *inst = getCxtE1(myInstance);
        AssertFatal(inst, "no E1 instance found for instance %ld\n", myInstance);
        e1apCUUP_send_BEARER_CONTEXT_SETUP_RESPONSE(inst->cuup.assoc_id, resp);
        free_e1ap_context_setup_response(resp);
      } break;

      case E1AP_BEARER_CONTEXT_MODIFICATION_RESP: {
        const e1ap_bearer_modif_resp_t *resp = &E1AP_BEARER_CONTEXT_MODIFICATION_RESP(msg);
        const e1ap_upcp_inst_t *inst = getCxtE1(myInstance);
        AssertFatal(inst, "no E1 instance found for instance %ld\n", myInstance);
        e1apCUUP_send_BEARER_CONTEXT_MODIFICATION_RESPONSE(inst->cuup.assoc_id, resp);
      } break;

      case E1AP_BEARER_CONTEXT_RELEASE_CPLT: {
        const e1ap_bearer_release_cplt_t *cplt = &E1AP_BEARER_CONTEXT_RELEASE_CPLT(msg);
        const e1ap_upcp_inst_t *inst = getCxtE1(myInstance);
        AssertFatal(inst, "no E1 instance found for instance %ld\n", myInstance);
        e1apCUUP_send_BEARER_CONTEXT_RELEASE_COMPLETE(inst->cuup.assoc_id, cplt);
      } break;

      default:
        LOG_E(E1AP, "Unknown message received in TASK_CUUP_E1\n");
        break;
    }

    result = itti_free(ITTI_MSG_ORIGIN_ID(msg), msg);
    AssertFatal(result == EXIT_SUCCESS, "Failed to free memory (%d) in E1AP_CUUP_task!\n", result);
    msg = NULL;
  }
}
