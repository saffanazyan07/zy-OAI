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

/*! \file e1ap_bearer_context_management.c
 * \brief E1AP library for E1 Bearer Context Management
 * \author Guido Casati
 * \date 2024
 * \version 0.1
 */

#include <string.h>

#include "common/utils/assertions.h"
#include "openair3/UTILS/conversions.h"
#include "common/utils/oai_asn1.h"
#include "common/utils/utils.h"

#include "e1ap_lib_common.h"
#include "e1ap_bearer_context_management.h"
#include "e1ap_lib_includes.h"

/* ====================================
 *   E1AP Bearer Context Setup Request
 * ==================================== */

/**
 * @brief Equality check for bearer_context_pdcp_config_t
 */
static bool eq_pdcp_config(const bearer_context_pdcp_config_t *a, const bearer_context_pdcp_config_t *b)
{
  _E1_EQ_CHECK_LONG(a->pDCP_SN_Size_UL, b->pDCP_SN_Size_UL);
  _E1_EQ_CHECK_LONG(a->pDCP_SN_Size_DL, b->pDCP_SN_Size_DL);
  _E1_EQ_CHECK_LONG(a->rLC_Mode, b->rLC_Mode);
  _E1_EQ_CHECK_LONG(a->reorderingTimer, b->reorderingTimer);
  _E1_EQ_CHECK_LONG(a->discardTimer, b->discardTimer);
  _E1_EQ_CHECK_INT(a->pDCP_Reestablishment, b->pDCP_Reestablishment);
  return true;
}

/**
 * @brief Equality check for bearer_context_sdap_config_t
 */
static bool eq_sdap_config(const bearer_context_sdap_config_t *a, const bearer_context_sdap_config_t *b)
{
  _E1_EQ_CHECK_LONG(a->defaultDRB, b->defaultDRB);
  _E1_EQ_CHECK_LONG(a->sDAP_Header_UL, b->sDAP_Header_UL);
  _E1_EQ_CHECK_LONG(a->sDAP_Header_DL, b->sDAP_Header_DL);
  return true;
}

/**
 * @brief Equality check for qos_flow_to_setup_t
 */
static bool eq_qos_flow(const qos_flow_to_setup_t *a, const qos_flow_to_setup_t *b)
{
  _E1_EQ_CHECK_LONG(a->qfi, b->qfi);
  const ngran_allocation_retention_priority_t *arp_a = &a->qos_params.alloc_reten_priority;
  const ngran_allocation_retention_priority_t *arp_b = &b->qos_params.alloc_reten_priority;
  _E1_EQ_CHECK_INT(arp_a->preemption_capability, arp_b->preemption_capability);
  _E1_EQ_CHECK_INT(arp_a->preemption_vulnerability, arp_b->preemption_vulnerability);
  _E1_EQ_CHECK_INT(arp_a->priority_level, arp_b->priority_level);
  const qos_characteristics_t *qos_a = &a->qos_params.qos_characteristics;
  const qos_characteristics_t *qos_b = &b->qos_params.qos_characteristics;
  _E1_EQ_CHECK_INT(qos_a->qos_type, qos_b->qos_type);
  _E1_EQ_CHECK_INT(qos_a->dynamic.fiveqi, qos_b->dynamic.fiveqi);
  _E1_EQ_CHECK_INT(qos_a->dynamic.packet_delay_budget, qos_b->dynamic.packet_delay_budget);
  _E1_EQ_CHECK_INT(qos_a->dynamic.packet_error_rate.per_exponent, qos_b->dynamic.packet_error_rate.per_exponent);
  _E1_EQ_CHECK_INT(qos_a->dynamic.packet_error_rate.per_scalar, qos_b->dynamic.packet_error_rate.per_scalar);
  _E1_EQ_CHECK_INT(qos_a->dynamic.qos_priority_level, qos_b->dynamic.qos_priority_level);
  _E1_EQ_CHECK_INT(qos_a->non_dynamic.fiveqi, qos_b->non_dynamic.fiveqi);
  _E1_EQ_CHECK_INT(qos_a->non_dynamic.qos_priority_level, qos_b->non_dynamic.qos_priority_level);
  return true;
}

/**
 * @brief Equality check for DRB_nGRAN_to_setup_t
 */
static bool eq_drb_to_setup(const DRB_nGRAN_to_setup_t *a, const DRB_nGRAN_to_setup_t *b)
{
  bool result = true;
  _E1_EQ_CHECK_LONG(a->id, b->id);
  _E1_EQ_CHECK_INT(a->numCellGroups, b->numCellGroups);
  for (int i = 0; i < a->numCellGroups; i++) {
    _E1_EQ_CHECK_INT(a->cellGroupList[i], b->cellGroupList[i]);
  }
  _E1_EQ_CHECK_INT(a->numQosFlow2Setup, b->numQosFlow2Setup);
  for (int i = 0; i < a->numQosFlow2Setup; i++) {
    result &= eq_qos_flow(&a->qosFlows[i], &b->qosFlows[i]);
  }
  result &= eq_pdcp_config(&a->pdcp_config, &b->pdcp_config);
  result &= eq_sdap_config(&a->sdap_config, &b->sdap_config);
  return result;
}

/**
 * @brief Equality check for DRB_nGRAN_to_mod_t
 */
static bool eq_drb_to_mod(const DRB_nGRAN_to_mod_t *a, const DRB_nGRAN_to_mod_t *b)
{
  bool result = true;
  _E1_EQ_CHECK_LONG(a->id, b->id);
  _E1_EQ_CHECK_INT(a->numCellGroups, b->numCellGroups);
  for (int i = 0; i < a->numCellGroups; i++) {
    _E1_EQ_CHECK_INT(a->cellGroupList[i], b->cellGroupList[i]);
  }
  _E1_EQ_CHECK_INT(a->numQosFlow2Setup, b->numQosFlow2Setup);
  for (int i = 0; i < a->numQosFlow2Setup; i++) {
    result &= eq_qos_flow(&a->qosFlows[i], &b->qosFlows[i]);
  }
  _E1_EQ_CHECK_INT(a->tlAddress, b->tlAddress);
  _E1_EQ_CHECK_INT(a->teId, b->teId);
  _E1_EQ_CHECK_INT(a->numDlUpParam, b->numDlUpParam);
  for (int i = 0; i < a->numDlUpParam; i++) {
    _E1_EQ_CHECK_INT(a->DlUpParamList[i].cell_group_id, b->DlUpParamList[i].cell_group_id);
    _E1_EQ_CHECK_LONG(a->DlUpParamList[i].teId, b->DlUpParamList[i].teId);
    _E1_EQ_CHECK_INT(a->DlUpParamList[i].tlAddress, b->DlUpParamList[i].tlAddress);
  }
  result &= eq_pdcp_config(&a->pdcp_config, &b->pdcp_config);
  result &= eq_sdap_config(&a->sdap_config, &b->sdap_config);
  return result;
}

/* PDU Session To Setup Item */

static bool e1_encode_pdu_session_to_setup_item(E1AP_PDU_Session_Resource_To_Setup_Item_t *item, const pdu_session_to_setup_t *in)
{
  item->pDU_Session_ID = in->sessionId;
  item->pDU_Session_Type = in->sessionType;
  // SNSSAI
  INT8_TO_OCTET_STRING(in->nssai.sst, &item->sNSSAI.sST);
  if (in->nssai.sd != 0xffffff) {
    item->sNSSAI.sD = malloc_or_fail(sizeof(*item->sNSSAI.sD));
    INT24_TO_OCTET_STRING(in->nssai.sd, item->sNSSAI.sD);
  }
  // Security Indication
  E1AP_SecurityIndication_t *siOut = &item->securityIndication;
  const security_indication_t *siIn = &in->securityIndication;
  siOut->integrityProtectionIndication = siIn->integrityProtectionIndication;
  siOut->confidentialityProtectionIndication = siIn->confidentialityProtectionIndication;
  if (siIn->integrityProtectionIndication != SECURITY_NOT_NEEDED) {
    asn1cCalloc(siOut->maximumIPdatarate, ipDataRate);
    ipDataRate->maxIPrate = siIn->maxIPrate;
  }
  // TNL Information
  item->nG_UL_UP_TNL_Information.present = E1AP_UP_TNL_Information_PR_gTPTunnel;
  asn1cCalloc(item->nG_UL_UP_TNL_Information.choice.gTPTunnel, gTPTunnel);
  const UP_TL_information_t *UP_TL_information = &in->UP_TL_information;
  TRANSPORT_LAYER_ADDRESS_IPv4_TO_BIT_STRING(UP_TL_information->tlAddress, &gTPTunnel->transportLayerAddress);
  INT32_TO_OCTET_STRING(UP_TL_information->teId, &gTPTunnel->gTP_TEID);
  // DRB to setup
  for (const DRB_nGRAN_to_setup_t *j = in->DRBnGRanList; j < in->DRBnGRanList + in->numDRB2Setup; j++) {
    asn1cSequenceAdd(item->dRB_To_Setup_List_NG_RAN.list, E1AP_DRB_To_Setup_Item_NG_RAN_t, ieC6_1_1);
    ieC6_1_1->dRB_ID = j->id;
    // SDAP Config
    E1AP_SDAP_Configuration_t *sdap = &ieC6_1_1->sDAP_Configuration;
    sdap->defaultDRB = j->sdap_config.defaultDRB ? E1AP_DefaultDRB_true : E1AP_DefaultDRB_false;
    sdap->sDAP_Header_UL = j->sdap_config.sDAP_Header_UL;
    sdap->sDAP_Header_DL = j->sdap_config.sDAP_Header_DL;
    // PDCP Config
    E1AP_PDCP_Configuration_t *pdcp = &ieC6_1_1->pDCP_Configuration;
    pdcp->pDCP_SN_Size_UL = j->pdcp_config.pDCP_SN_Size_UL;
    pdcp->pDCP_SN_Size_DL = j->pdcp_config.pDCP_SN_Size_DL;
    asn1cCallocOne(pdcp->discardTimer, j->pdcp_config.discardTimer);
    E1AP_T_ReorderingTimer_t *roTimer = calloc_or_fail(1, sizeof(*roTimer));
    pdcp->t_ReorderingTimer = roTimer;
    roTimer->t_Reordering = j->pdcp_config.reorderingTimer;
    pdcp->rLC_Mode = j->pdcp_config.rLC_Mode;
    // Cell Group
    for (const cell_group_id_t *k = j->cellGroupList; k < j->cellGroupList + j->numCellGroups; k++) {
      asn1cSequenceAdd(ieC6_1_1->cell_Group_Information.list, E1AP_Cell_Group_Information_Item_t, ieC6_1_1_1);
      ieC6_1_1_1->cell_Group_ID = *k;
    }
    // QoS Flows
    for (const qos_flow_to_setup_t *k = j->qosFlows; k < j->qosFlows + j->numQosFlow2Setup; k++) {
      asn1cSequenceAdd(ieC6_1_1->qos_flow_Information_To_Be_Setup, E1AP_QoS_Flow_QoS_Parameter_Item_t, ieC6_1_1_1);
      ieC6_1_1_1->qoS_Flow_Identifier = k->qfi;
      // QoS Characteristics
      const qos_characteristics_t *qos_char_in = &k->qos_params.qos_characteristics;
      if (qos_char_in->qos_type == NON_DYNAMIC) { // non Dynamic 5QI
        ieC6_1_1_1->qoSFlowLevelQoSParameters.qoS_Characteristics.present = E1AP_QoS_Characteristics_PR_non_Dynamic_5QI;
        asn1cCalloc(ieC6_1_1_1->qoSFlowLevelQoSParameters.qoS_Characteristics.choice.non_Dynamic_5QI, non_Dynamic_5QI);
        non_Dynamic_5QI->fiveQI = qos_char_in->non_dynamic.fiveqi;
      } else { // dynamic 5QI
        ieC6_1_1_1->qoSFlowLevelQoSParameters.qoS_Characteristics.present = E1AP_QoS_Characteristics_PR_dynamic_5QI;
        asn1cCalloc(ieC6_1_1_1->qoSFlowLevelQoSParameters.qoS_Characteristics.choice.dynamic_5QI, dynamic_5QI);
        dynamic_5QI->qoSPriorityLevel = qos_char_in->dynamic.qos_priority_level;
        dynamic_5QI->packetDelayBudget = qos_char_in->dynamic.packet_delay_budget;
        dynamic_5QI->packetErrorRate.pER_Scalar = qos_char_in->dynamic.packet_error_rate.per_scalar;
        dynamic_5QI->packetErrorRate.pER_Exponent = qos_char_in->dynamic.packet_error_rate.per_exponent;
      }
      // QoS Retention Priority
      const ngran_allocation_retention_priority_t *rent_priority_in = &k->qos_params.alloc_reten_priority;
      E1AP_NGRANAllocationAndRetentionPriority_t *arp = &ieC6_1_1_1->qoSFlowLevelQoSParameters.nGRANallocationRetentionPriority;
      arp->priorityLevel = rent_priority_in->priority_level;
      arp->pre_emptionCapability = rent_priority_in->preemption_capability;
      arp->pre_emptionVulnerability = rent_priority_in->preemption_vulnerability;
    }
  }
  return true;
}

static bool e1_decode_pdu_session_to_setup_item(pdu_session_to_setup_t *out, E1AP_PDU_Session_Resource_To_Setup_Item_t *item)
{
  // PDU Session ID (M)
  out->sessionId = item->pDU_Session_ID;
  // PDU Session Type (M)
  out->sessionType = item->pDU_Session_Type;
  /* S-NSSAI (M) */
  // SST (M)
  OCTET_STRING_TO_INT8(&item->sNSSAI.sST, out->nssai.sst);
  out->nssai.sd = 0xffffff;
  // SD (O)
  if (item->sNSSAI.sD != NULL)
    OCTET_STRING_TO_INT24(item->sNSSAI.sD, out->nssai.sd);
  /* Security Indication (M) */
  E1AP_SecurityIndication_t *securityIndication = &item->securityIndication;
  // Integrity Protection Indication (M)
  security_indication_t *sec = &out->securityIndication;
  sec->integrityProtectionIndication = securityIndication->integrityProtectionIndication;
  // Confidentiality Protection Indication (M)
  sec->confidentialityProtectionIndication = securityIndication->confidentialityProtectionIndication;
  // Maximum Integrity Protected Data Rate (Conditional)
  if (sec->integrityProtectionIndication != SECURITY_NOT_NEEDED) {
    if (securityIndication->maximumIPdatarate)
      sec->maxIPrate = securityIndication->maximumIPdatarate->maxIPrate;
    else {
      PRINT_ERROR("Received integrityProtectionIndication but maximumIPdatarate IE is missing\n");
      return false;
    }
  }
  /* NG UL UP Transport Layer Information (M) (9.3.2.1 of 3GPP TS 38.463) */
  // GTP Tunnel
  struct E1AP_GTPTunnel *gTPTunnel = item->nG_UL_UP_TNL_Information.choice.gTPTunnel;
  if (!gTPTunnel) {
    PRINT_ERROR("Missing mandatory IE item->nG_UL_UP_TNL_Information.choice.gTPTunnel\n");
    return false;
  }
  _E1_EQ_CHECK_INT(item->nG_UL_UP_TNL_Information.present, E1AP_UP_TNL_Information_PR_gTPTunnel);
  // Transport Layer Address
  UP_TL_information_t *UP_TL_information = &out->UP_TL_information;
  BIT_STRING_TO_TRANSPORT_LAYER_ADDRESS_IPv4(&gTPTunnel->transportLayerAddress, UP_TL_information->tlAddress);
  // GTP-TEID
  OCTET_STRING_TO_INT32(&gTPTunnel->gTP_TEID, UP_TL_information->teId);
  // DRB To Setup List ( > 1 item )
  E1AP_DRB_To_Setup_List_NG_RAN_t *drb2SetupList = &item->dRB_To_Setup_List_NG_RAN;
  _E1_EQ_CHECK_GENERIC(drb2SetupList->list.count > 0, "%d", drb2SetupList->list.count, 0);
  out->numDRB2Setup = drb2SetupList->list.count;
  _E1_EQ_CHECK_INT(out->numDRB2Setup, 1); // can only handle one DRB per PDU session
  for (int j = 0; j < drb2SetupList->list.count; j++) {
    DRB_nGRAN_to_setup_t *drb = out->DRBnGRanList + j;
    E1AP_DRB_To_Setup_Item_NG_RAN_t *drb2Setup = drb2SetupList->list.array[j];
    // DRB ID (M)
    drb->id = drb2Setup->dRB_ID;
    // >SDAP Configuration (M)
    E1AP_SDAP_Configuration_t *sdap = &drb2Setup->sDAP_Configuration;
    drb->sdap_config.defaultDRB = sdap->defaultDRB == E1AP_DefaultDRB_true;
    drb->sdap_config.sDAP_Header_UL = sdap->sDAP_Header_UL;
    drb->sdap_config.sDAP_Header_DL = sdap->sDAP_Header_DL;
    // PDCP Configuration (M)
    E1AP_PDCP_Configuration_t *pdcp = &drb2Setup->pDCP_Configuration;
    drb->pdcp_config.pDCP_SN_Size_UL = pdcp->pDCP_SN_Size_UL;
    drb->pdcp_config.pDCP_SN_Size_DL = pdcp->pDCP_SN_Size_DL;
    if (pdcp->discardTimer) {
      drb->pdcp_config.discardTimer = *pdcp->discardTimer;
    }
    if (pdcp->t_ReorderingTimer) {
      drb->pdcp_config.reorderingTimer = pdcp->t_ReorderingTimer->t_Reordering;
    }
    drb->pdcp_config.rLC_Mode = pdcp->rLC_Mode;
    // Cell Group Information (M) ( > 1 item )
    E1AP_Cell_Group_Information_t *cellGroupList = &drb2Setup->cell_Group_Information;
    _E1_EQ_CHECK_GENERIC(cellGroupList->list.count > 0, "%d", cellGroupList->list.count, 0);
    drb->numCellGroups = cellGroupList->list.count;
    for (int k = 0; k < cellGroupList->list.count; k++) {
      E1AP_Cell_Group_Information_Item_t *cg2Setup = cellGroupList->list.array[k];
      // Cell Group ID
      drb->cellGroupList[k] = cg2Setup->cell_Group_ID;
    }
    // QoS Flows Information To Be Setup (M)
    E1AP_QoS_Flow_QoS_Parameter_List_t *qos2SetupList = &drb2Setup->qos_flow_Information_To_Be_Setup;
    drb->numQosFlow2Setup = qos2SetupList->list.count;
    for (int k = 0; k < qos2SetupList->list.count; k++) {
      qos_flow_to_setup_t *qos_flow = drb->qosFlows + k;
      E1AP_QoS_Flow_QoS_Parameter_Item_t *qos2Setup = qos2SetupList->list.array[k];
      // QoS Flow Identifier (M)
      qos_flow->qfi = qos2Setup->qoS_Flow_Identifier;
      qos_characteristics_t *qos_char = &qos_flow->qos_params.qos_characteristics;
      // QoS Flow Level QoS Parameters (M)
      E1AP_QoSFlowLevelQoSParameters_t *qosParams = &qos2Setup->qoSFlowLevelQoSParameters;
      E1AP_QoS_Characteristics_t *qoS_Characteristics = &qosParams->qoS_Characteristics;
      switch (qoS_Characteristics->present) {
        case E1AP_QoS_Characteristics_PR_non_Dynamic_5QI:
          qos_char->qos_type = NON_DYNAMIC;
          qos_char->non_dynamic.fiveqi = qoS_Characteristics->choice.non_Dynamic_5QI->fiveQI;
          break;
        case E1AP_QoS_Characteristics_PR_dynamic_5QI: {
          E1AP_Dynamic5QIDescriptor_t *dynamic5QI = qoS_Characteristics->choice.dynamic_5QI;
          qos_char->qos_type = DYNAMIC;
          qos_char->dynamic.qos_priority_level = dynamic5QI->qoSPriorityLevel;
          qos_char->dynamic.packet_delay_budget = dynamic5QI->packetDelayBudget;
          qos_char->dynamic.packet_error_rate.per_scalar = dynamic5QI->packetErrorRate.pER_Scalar;
          qos_char->dynamic.packet_error_rate.per_exponent = dynamic5QI->packetErrorRate.pER_Exponent;
          break;
        }
        default:
          PRINT_ERROR("Unexpected QoS Characteristics type: %d\n", qoS_Characteristics->present);
          return false;
          break;
      }
      // NG-RAN Allocation and Retention Priority (M)
      ngran_allocation_retention_priority_t *rent_priority = &qos_flow->qos_params.alloc_reten_priority;
      E1AP_NGRANAllocationAndRetentionPriority_t *aRP = &qosParams->nGRANallocationRetentionPriority;
      rent_priority->priority_level = aRP->priorityLevel;
      rent_priority->preemption_capability = aRP->pre_emptionCapability;
      rent_priority->preemption_vulnerability = aRP->pre_emptionVulnerability;
    }
  }
  return true;
}

/**
 * @brief Equality check for PDU session item to modify/setup
 */
static bool eq_pdu_session_item(const pdu_session_to_setup_t *a, const pdu_session_to_setup_t *b)
{
  _E1_EQ_CHECK_LONG(a->sessionId, b->sessionId);
  _E1_EQ_CHECK_LONG(a->sessionType, b->sessionType);
  _E1_EQ_CHECK_INT(a->UP_TL_information.tlAddress, b->UP_TL_information.tlAddress);
  _E1_EQ_CHECK_INT(a->UP_TL_information.teId, b->UP_TL_information.teId);
  _E1_EQ_CHECK_LONG(a->numDRB2Setup, b->numDRB2Setup);
  if (a->numDRB2Setup != b->numDRB2Setup)
    return false;
  for (int i = 0; i < a->numDRB2Setup; i++)
    if (!eq_drb_to_setup(&a->DRBnGRanList[i], &b->DRBnGRanList[i]))
      return false;
  _E1_EQ_CHECK_LONG(a->numDRB2Modify, b->numDRB2Modify);
  if (a->numDRB2Modify != b->numDRB2Modify)
    return false;
  for (int i = 0; i < a->numDRB2Modify; i++)
    if (!eq_drb_to_mod(&a->DRBnGRanModList[i], &b->DRBnGRanModList[i]))
      return false;
  return true;
}

static bool cp_pdu_session_item(pdu_session_to_setup_t *cp, const pdu_session_to_setup_t *msg)
{
  // Copy basic fields
  *cp = *msg;
  // Copy DRB to Setup List
  for (int j = 0; j < msg->numDRB2Setup; j++) {
    cp->DRBnGRanList[j].id = msg->DRBnGRanList[j].id;
    cp->DRBnGRanList[j].sdap_config = msg->DRBnGRanList[j].sdap_config;
    cp->DRBnGRanList[j].pdcp_config = msg->DRBnGRanList[j].pdcp_config;
    cp->DRBnGRanList[j].tlAddress = msg->DRBnGRanList[j].tlAddress;
    cp->DRBnGRanList[j].teId = msg->DRBnGRanList[j].teId;
    cp->DRBnGRanList[j].numDlUpParam = msg->DRBnGRanList[j].numDlUpParam;
    // Copy DL UP Params List
    for (int k = 0; k < msg->DRBnGRanList[j].numDlUpParam; k++) {
      cp->DRBnGRanList[j].DlUpParamList[k] = msg->DRBnGRanList[j].DlUpParamList[k];
    }
    cp->DRBnGRanList[j].numCellGroups = msg->DRBnGRanList[j].numCellGroups;
    // Copy Cell Group List
    for (int k = 0; k < msg->DRBnGRanList[j].numCellGroups; k++) {
      cp->DRBnGRanList[j].cellGroupList[k] = msg->DRBnGRanList[j].cellGroupList[k];
    }
    cp->DRBnGRanList[j].numQosFlow2Setup = msg->DRBnGRanList[j].numQosFlow2Setup;
    // Copy QoS Flow To Setup List
    for (int k = 0; k < msg->DRBnGRanList[j].numQosFlow2Setup; k++) {
      cp->DRBnGRanList[j].qosFlows[k].qfi = msg->DRBnGRanList[j].qosFlows[k].qfi;
      cp->DRBnGRanList[j].qosFlows[k].qos_params = msg->DRBnGRanList[j].qosFlows[k].qos_params;
    }
  }
  // Copy DRB to Modify List
  for (int j = 0; j < msg->numDRB2Modify; j++) {
    cp->DRBnGRanModList[j] = msg->DRBnGRanModList[j];
  }
  return true;
}

/**
 * @brief E1AP Bearer Context Setup Request encoding (9.2.2 of 3GPP TS 38.463)
 */
E1AP_E1AP_PDU_t *encode_E1_bearer_context_setup_request(const e1ap_bearer_setup_req_t *msg)
{
  E1AP_E1AP_PDU_t *pdu = calloc_or_fail(1, sizeof(*pdu));

  pdu->present = E1AP_E1AP_PDU_PR_initiatingMessage;
  asn1cCalloc(pdu->choice.initiatingMessage, initMsg);
  initMsg->procedureCode = E1AP_ProcedureCode_id_bearerContextSetup;
  initMsg->criticality = E1AP_Criticality_reject;
  initMsg->value.present = E1AP_InitiatingMessage__value_PR_BearerContextSetupRequest;
  E1AP_BearerContextSetupRequest_t *out = &pdu->choice.initiatingMessage->value.choice.BearerContextSetupRequest;
  /* mandatory */
  /* c1. gNB-CU-UP UE E1AP ID */
  asn1cSequenceAdd(out->protocolIEs.list, E1AP_BearerContextSetupRequestIEs_t, ieC1);
  ieC1->id = E1AP_ProtocolIE_ID_id_gNB_CU_CP_UE_E1AP_ID;
  ieC1->criticality = E1AP_Criticality_reject;
  ieC1->value.present = E1AP_BearerContextSetupRequestIEs__value_PR_GNB_CU_CP_UE_E1AP_ID;
  ieC1->value.choice.GNB_CU_CP_UE_E1AP_ID = msg->gNB_cu_cp_ue_id;
  /* mandatory */
  /* c2. Security Information */
  asn1cSequenceAdd(out->protocolIEs.list, E1AP_BearerContextSetupRequestIEs_t, ieC2);
  ieC2->id = E1AP_ProtocolIE_ID_id_SecurityInformation;
  ieC2->criticality = E1AP_Criticality_reject;
  ieC2->value.present = E1AP_BearerContextSetupRequestIEs__value_PR_SecurityInformation;
  E1AP_SecurityAlgorithm_t *securityAlgorithm = &ieC2->value.choice.SecurityInformation.securityAlgorithm;
  E1AP_UPSecuritykey_t *uPSecuritykey = &ieC2->value.choice.SecurityInformation.uPSecuritykey;
  securityAlgorithm->cipheringAlgorithm = msg->cipheringAlgorithm;
  OCTET_STRING_fromBuf(&uPSecuritykey->encryptionKey, msg->encryptionKey, E1AP_SECURITY_KEY_SIZE);
  asn1cCallocOne(securityAlgorithm->integrityProtectionAlgorithm, msg->integrityProtectionAlgorithm);
  asn1cCalloc(uPSecuritykey->integrityProtectionKey, protKey);
  OCTET_STRING_fromBuf(protKey, msg->integrityProtectionKey, E1AP_SECURITY_KEY_SIZE);
  /* mandatory */
  /* c3. UE DL Aggregate Maximum Bit Rate */
  asn1cSequenceAdd(out->protocolIEs.list, E1AP_BearerContextSetupRequestIEs_t, ieC3);
  ieC3->id = E1AP_ProtocolIE_ID_id_UEDLAggregateMaximumBitRate;
  ieC3->criticality = E1AP_Criticality_reject;
  ieC3->value.present = E1AP_BearerContextSetupRequestIEs__value_PR_BitRate;
  asn_long2INTEGER(&ieC3->value.choice.BitRate, msg->ueDlAggMaxBitRate);
  /* mandatory */
  /* c4. Serving PLMN */
  asn1cSequenceAdd(out->protocolIEs.list, E1AP_BearerContextSetupRequestIEs_t, ieC4);
  ieC4->id = E1AP_ProtocolIE_ID_id_Serving_PLMN;
  ieC4->criticality = E1AP_Criticality_ignore;
  ieC4->value.present = E1AP_BearerContextSetupRequestIEs__value_PR_PLMN_Identity;
  const PLMN_ID_t *servingPLMN = &msg->servingPLMNid;
  MCC_MNC_TO_PLMNID(servingPLMN->mcc, servingPLMN->mnc, servingPLMN->mnc_digit_length, &ieC4->value.choice.PLMN_Identity);
  /* mandatory */
  /* Activity Notification Level */
  asn1cSequenceAdd(out->protocolIEs.list, E1AP_BearerContextSetupRequestIEs_t, ieC5);
  ieC5->id = E1AP_ProtocolIE_ID_id_ActivityNotificationLevel;
  ieC5->criticality = E1AP_Criticality_reject;
  ieC5->value.present = E1AP_BearerContextSetupRequestIEs__value_PR_ActivityNotificationLevel;
  ieC5->value.choice.ActivityNotificationLevel = E1AP_ActivityNotificationLevel_pdu_session; // TODO: Remove hard coding
  /* mandatory */
  /*  */
  asn1cSequenceAdd(out->protocolIEs.list, E1AP_BearerContextSetupRequestIEs_t, ieC6);
  ieC6->id = E1AP_ProtocolIE_ID_id_System_BearerContextSetupRequest;
  ieC6->criticality = E1AP_Criticality_reject;
  ieC6->value.present = E1AP_BearerContextSetupRequestIEs__value_PR_System_BearerContextSetupRequest;
  ieC6->value.choice.System_BearerContextSetupRequest.present =
      E1AP_System_BearerContextSetupRequest_PR_nG_RAN_BearerContextSetupRequest;
  E1AP_ProtocolIE_Container_4932P19_t *msgNGRAN_list = calloc_or_fail(1, sizeof(*msgNGRAN_list));
  ieC6->value.choice.System_BearerContextSetupRequest.choice.nG_RAN_BearerContextSetupRequest =
      (struct E1AP_ProtocolIE_Container *)msgNGRAN_list;
  asn1cSequenceAdd(msgNGRAN_list->list, E1AP_NG_RAN_BearerContextSetupRequest_t, msgNGRAN);
  msgNGRAN->id = E1AP_ProtocolIE_ID_id_PDU_Session_Resource_To_Setup_List;
  msgNGRAN->criticality = E1AP_Criticality_reject;
  msgNGRAN->value.present = E1AP_NG_RAN_BearerContextSetupRequest__value_PR_PDU_Session_Resource_To_Setup_List;
  E1AP_PDU_Session_Resource_To_Setup_List_t *pdu2Setup = &msgNGRAN->value.choice.PDU_Session_Resource_To_Setup_List;
  for (int i = 0; i < msg->numPDUSessions; i++) {
    const pdu_session_to_setup_t *pdu_session = &msg->pduSession[i];
    asn1cSequenceAdd(pdu2Setup->list, E1AP_PDU_Session_Resource_To_Setup_Item_t, ieC6_1);
    e1_encode_pdu_session_to_setup_item(ieC6_1, pdu_session);
  }
  return pdu;
}

/**
 * @brief E1AP Bearer Context Setup Request memory management
 */
void free_e1ap_context_setup_request(e1ap_bearer_setup_req_t *msg)
{
  // Do nothing
}

/**
 * @brief E1AP Bearer Context Setup Request decoding
 *        gNB-CU-CP → gNB-CU-UP (9.2.2.1 of 3GPP TS 38.463)
 */
bool decode_E1_bearer_context_setup_request(const E1AP_E1AP_PDU_t *pdu, e1ap_bearer_setup_req_t *out)
{
  DevAssert(pdu != NULL);

  // Check message type
  _E1_EQ_CHECK_INT(pdu->present,E1AP_E1AP_PDU_PR_initiatingMessage);
  _E1_EQ_CHECK_INT(pdu->choice.initiatingMessage->procedureCode, E1AP_ProcedureCode_id_bearerContextSetup);
  _E1_EQ_CHECK_INT(pdu->choice.initiatingMessage->criticality, E1AP_Criticality_reject);
  _E1_EQ_CHECK_INT(pdu->choice.initiatingMessage->value.present, E1AP_InitiatingMessage__value_PR_BearerContextSetupRequest);

  const E1AP_BearerContextSetupRequest_t *in = &pdu->choice.initiatingMessage->value.choice.BearerContextSetupRequest;
  E1AP_BearerContextSetupRequestIEs_t *ie;

  // Check mandatory IEs first
  E1AP_LIB_FIND_IE(E1AP_BearerContextSetupRequestIEs_t, ie, in, E1AP_ProtocolIE_ID_id_gNB_CU_CP_UE_E1AP_ID, true);
  E1AP_LIB_FIND_IE(E1AP_BearerContextSetupRequestIEs_t, ie, in, E1AP_ProtocolIE_ID_id_SecurityInformation, true);
  E1AP_LIB_FIND_IE(E1AP_BearerContextSetupRequestIEs_t, ie, in, E1AP_ProtocolIE_ID_id_UEDLAggregateMaximumBitRate, true);
  E1AP_LIB_FIND_IE(E1AP_BearerContextSetupRequestIEs_t, ie, in, E1AP_ProtocolIE_ID_id_Serving_PLMN, true);
  E1AP_LIB_FIND_IE(E1AP_BearerContextSetupRequestIEs_t, ie, in, E1AP_ProtocolIE_ID_id_ActivityNotificationLevel, true);
  E1AP_LIB_FIND_IE(E1AP_BearerContextSetupRequestIEs_t, ie, in, E1AP_ProtocolIE_ID_id_System_BearerContextSetupRequest, true);

  // Loop over all IEs
  for (int i = 0; i < in->protocolIEs.list.count; i++) {
    ie = in->protocolIEs.list.array[i];
    AssertFatal(ie != NULL, "in->protocolIEs.list.array[i] shall not be null");
    switch (ie->id) {
      case E1AP_ProtocolIE_ID_id_gNB_CU_CP_UE_E1AP_ID:
        _E1_EQ_CHECK_INT(ie->value.present, E1AP_BearerContextSetupRequestIEs__value_PR_GNB_CU_CP_UE_E1AP_ID);
        out->gNB_cu_cp_ue_id = ie->value.choice.GNB_CU_CP_UE_E1AP_ID;
        break;

      case E1AP_ProtocolIE_ID_id_SecurityInformation:
        _E1_EQ_CHECK_INT(ie->value.present, E1AP_BearerContextSetupRequestIEs__value_PR_SecurityInformation);
        E1AP_SecurityInformation_t *SecurityInformation = &ie->value.choice.SecurityInformation;
        E1AP_SecurityAlgorithm_t *securityAlgorithm = &SecurityInformation->securityAlgorithm;
        E1AP_EncryptionKey_t *encryptionKey = &SecurityInformation->uPSecuritykey.encryptionKey;
        E1AP_IntegrityProtectionKey_t *integrityProtectionKey = SecurityInformation->uPSecuritykey.integrityProtectionKey;

        out->cipheringAlgorithm = securityAlgorithm->cipheringAlgorithm;
        memcpy(out->encryptionKey, encryptionKey->buf, encryptionKey->size);
        if (securityAlgorithm->integrityProtectionAlgorithm)
          out->integrityProtectionAlgorithm = *securityAlgorithm->integrityProtectionAlgorithm;
        if (integrityProtectionKey)
          memcpy(out->integrityProtectionKey, integrityProtectionKey->buf, integrityProtectionKey->size);
        break;

      case E1AP_ProtocolIE_ID_id_UEDLAggregateMaximumBitRate:
        _E1_EQ_CHECK_INT(ie->value.present, E1AP_BearerContextSetupRequestIEs__value_PR_BitRate);
        asn_INTEGER2long(&ie->value.choice.BitRate, &out->ueDlAggMaxBitRate);
        break;

      case E1AP_ProtocolIE_ID_id_Serving_PLMN:
        _E1_EQ_CHECK_INT(ie->value.present, E1AP_BearerContextSetupRequestIEs__value_PR_PLMN_Identity);
        PLMNID_TO_MCC_MNC(&ie->value.choice.PLMN_Identity,
                          out->servingPLMNid.mcc,
                          out->servingPLMNid.mnc,
                          out->servingPLMNid.mnc_digit_length);
        break;

      case E1AP_ProtocolIE_ID_id_ActivityNotificationLevel:
        _E1_EQ_CHECK_INT(ie->value.present, E1AP_BearerContextSetupRequestIEs__value_PR_ActivityNotificationLevel);
        DevAssert(ie->value.present == E1AP_BearerContextSetupRequestIEs__value_PR_ActivityNotificationLevel);
        out->activityNotificationLevel = ie->value.choice.ActivityNotificationLevel;
        break;

      case E1AP_ProtocolIE_ID_id_System_BearerContextSetupRequest:
        _E1_EQ_CHECK_INT(ie->value.present, E1AP_BearerContextSetupRequestIEs__value_PR_System_BearerContextSetupRequest);
        E1AP_System_BearerContextSetupRequest_t *System_BearerContextSetupRequest =
            &ie->value.choice.System_BearerContextSetupRequest;
        switch (System_BearerContextSetupRequest->present) {
          case E1AP_System_BearerContextSetupRequest_PR_NOTHING:
            PRINT_ERROR("System Bearer Context Setup Request: no choice present\n");
            break;
          case E1AP_System_BearerContextSetupRequest_PR_e_UTRAN_BearerContextSetupRequest:
            AssertError(System_BearerContextSetupRequest->present == E1AP_System_BearerContextSetupRequest_PR_e_UTRAN_BearerContextSetupRequest,
                        return false, "E1AP_System_BearerContextSetupRequest_PR_e_UTRAN_BearerContextSetupRequest in E1 Setup Request not supported");
            break;
          case E1AP_System_BearerContextSetupRequest_PR_nG_RAN_BearerContextSetupRequest:
            // Handle nG-RAN Bearer Context Setup Request
            _E1_EQ_CHECK_INT(System_BearerContextSetupRequest->present,
                             E1AP_System_BearerContextSetupRequest_PR_nG_RAN_BearerContextSetupRequest);
            AssertFatal(System_BearerContextSetupRequest->choice.nG_RAN_BearerContextSetupRequest != NULL,
                        "System_BearerContextSetupRequest->choice.nG_RAN_BearerContextSetupRequest shall not be null");
            break;
          default:
            PRINT_ERROR("System Bearer Context Setup Request: No valid choice present.\n");
            break;
        }

        E1AP_ProtocolIE_Container_4932P19_t *msgNGRAN_list =
            (E1AP_ProtocolIE_Container_4932P19_t *)System_BearerContextSetupRequest->choice.nG_RAN_BearerContextSetupRequest;
        E1AP_NG_RAN_BearerContextSetupRequest_t *msgNGRAN = msgNGRAN_list->list.array[0];
        // NG-RAN
        _E1_EQ_CHECK_INT(msgNGRAN_list->list.count, 1); // only one RAN is expected
        _E1_EQ_CHECK_LONG(msgNGRAN->id, E1AP_ProtocolIE_ID_id_PDU_Session_Resource_To_Setup_List);
        _E1_EQ_CHECK_INT(msgNGRAN->value.present,
                         E1AP_NG_RAN_BearerContextSetupRequest__value_PR_PDU_Session_Resource_To_Setup_List);
        // PDU Session Resource To Setup List (9.3.3.2 of 3GPP TS 38.463)
        E1AP_PDU_Session_Resource_To_Setup_List_t *pdu2SetupList = &msgNGRAN->value.choice.PDU_Session_Resource_To_Setup_List;
        out->numPDUSessions = pdu2SetupList->list.count;
        // Loop through all PDU sessions
        for (int i = 0; i < pdu2SetupList->list.count; i++) {
          pdu_session_to_setup_t *pdu_session = out->pduSession + i;
          E1AP_PDU_Session_Resource_To_Setup_Item_t *pdu2Setup = pdu2SetupList->list.array[i];
          if (!e1_decode_pdu_session_to_setup_item(pdu_session, pdu2Setup)) {
            PRINT_ERROR("Failed to decode PDU Session to setup item %d\n", i);
            return false;
          }
        }
        break;
      default:
        PRINT_ERROR("Handle for this IE %ld is not implemented (or) invalid IE detected\n", ie->id);
        break;
    }
  }
  return true;
}

/**
 * @brief Deep copy function for E1 BEARER CONTEXT SETUP REQUEST
 */
e1ap_bearer_setup_req_t cp_bearer_context_setup_request(const e1ap_bearer_setup_req_t *msg)
{
  e1ap_bearer_setup_req_t cp = {0};
  // Copy basi fields
  cp = *msg;
  strncpy(cp.encryptionKey, msg->encryptionKey, sizeof(cp.encryptionKey));
  strncpy(cp.integrityProtectionKey, msg->integrityProtectionKey, sizeof(cp.integrityProtectionKey));
  // Copy PDU Sessions
  for (int i = 0; i < msg->numPDUSessions; i++)
    cp_pdu_session_item(&cp.pduSession[i], &msg->pduSession[i]);
  return cp;
}

/**
 * @brief E1AP Bearer Context Setup Request equality check
 */
bool eq_bearer_context_setup_request(const e1ap_bearer_setup_req_t *a, const e1ap_bearer_setup_req_t *b)
{
  // Primitive data types
  _E1_EQ_CHECK_INT(a->gNB_cu_cp_ue_id, b->gNB_cu_cp_ue_id);
  _E1_EQ_CHECK_LONG(a->cipheringAlgorithm, b->cipheringAlgorithm);
  _E1_EQ_CHECK_LONG(a->integrityProtectionAlgorithm, b->integrityProtectionAlgorithm);
  _E1_EQ_CHECK_LONG(a->ueDlAggMaxBitRate, b->ueDlAggMaxBitRate);
  _E1_EQ_CHECK_INT(a->activityNotificationLevel, b->activityNotificationLevel);
  _E1_EQ_CHECK_INT(a->numPDUSessions, b->numPDUSessions);
  _E1_EQ_CHECK_INT(a->numPDUSessionsMod, b->numPDUSessionsMod);
  // PLMN
  _E1_EQ_CHECK_INT(a->servingPLMNid.mcc, b->servingPLMNid.mcc);
  _E1_EQ_CHECK_INT(a->servingPLMNid.mnc, b->servingPLMNid.mnc);
  _E1_EQ_CHECK_INT(a->servingPLMNid.mnc_digit_length, b->servingPLMNid.mnc_digit_length);
  // Security Keys
  _E1_EQ_CHECK_STR(a->encryptionKey, b->encryptionKey);
  _E1_EQ_CHECK_STR(a->integrityProtectionKey, b->integrityProtectionKey);
  // PDU Sessions
  if (a->numPDUSessions != b->numPDUSessions)
    return false;
  for (int i = 0; i < a->numPDUSessions; i++)
    if (!eq_pdu_session_item(&a->pduSession[i], &b->pduSession[i]))
      return false;
  if (a->numPDUSessionsMod != b->numPDUSessionsMod)
    return false;
  for (int i = 0; i < a->numPDUSessionsMod; i++)
    if (!eq_pdu_session_item(&a->pduSessionMod[i], &b->pduSessionMod[i]))
      return false;
  return true;
}

/* ======================================
 *   E1AP Bearer Context Setup Response
 * ====================================== */

/* PDU Session Resource Setup Item */

/**
 * @brief Decode an item of the E1 PDU Session Resource Setup List (9.3.3.5 of 3GPP TS 38.463)
 */
static bool e1_decode_pdu_session_setup_item(pdu_session_setup_t *pduSetup, E1AP_PDU_Session_Resource_Setup_Item_t *in)
{
  // PDU Session ID
  pduSetup->id = in->pDU_Session_ID;
  pduSetup->numDRBSetup = in->dRB_Setup_List_NG_RAN.list.count;
  // NG DL UP Transport Layer Information (M)
  E1AP_UP_TNL_Information_t *DL_UP_TNL_Info = &in->nG_DL_UP_TNL_Information;
  if (DL_UP_TNL_Info->choice.gTPTunnel) {
    _E1_EQ_CHECK_INT(DL_UP_TNL_Info->present, E1AP_UP_TNL_Information_PR_gTPTunnel);
    BIT_STRING_TO_TRANSPORT_LAYER_ADDRESS_IPv4(&DL_UP_TNL_Info->choice.gTPTunnel->transportLayerAddress, pduSetup->tlAddress);
    OCTET_STRING_TO_INT32(&DL_UP_TNL_Info->choice.gTPTunnel->gTP_TEID, pduSetup->teId);
  }
  // DRB Setup List (1..<maxnoofDRBs>)
  for (int j = 0; j < in->dRB_Setup_List_NG_RAN.list.count; j++) {
    DRB_nGRAN_setup_t *drbSetup = pduSetup->DRBnGRanList + j;
    E1AP_DRB_Setup_Item_NG_RAN_t *drb = in->dRB_Setup_List_NG_RAN.list.array[j];
    // DRB ID (M)
    drbSetup->id = drb->dRB_ID;
    // UL UP Parameters (M)
    drbSetup->numUpParam = drb->uL_UP_Transport_Parameters.list.count;
    for (int k = 0; k < drb->uL_UP_Transport_Parameters.list.count; k++) {
      up_params_t *UL_UP_param = drbSetup->UpParamList + k;
      // UP Parameters List (1..<maxnoofUPParameters>)
      E1AP_UP_Parameters_Item_t *UL_UP_item = drb->uL_UP_Transport_Parameters.list.array[k];
      // UP Transport Layer Information (M)
      DevAssert(UL_UP_item->uP_TNL_Information.present == E1AP_UP_TNL_Information_PR_gTPTunnel);
      // GTP Tunnel (M)
      E1AP_GTPTunnel_t *gTPTunnel = UL_UP_item->uP_TNL_Information.choice.gTPTunnel;
      AssertError(gTPTunnel != NULL, return false, "gTPTunnel information in required in UP Transport Layer Information\n");
      if (gTPTunnel) {
        BIT_STRING_TO_TRANSPORT_LAYER_ADDRESS_IPv4(&gTPTunnel->transportLayerAddress, UL_UP_param->tlAddress);
        OCTET_STRING_TO_INT32(&gTPTunnel->gTP_TEID, UL_UP_param->teId);
      } else {
      }
      // Cell Group ID (M)
      UL_UP_param->cell_group_id = UL_UP_item->cell_Group_ID;
    }
    // Flow Setup List (M)
    drbSetup->numQosFlowSetup = drb->flow_Setup_List.list.count;
    for (int q = 0; q < drb->flow_Setup_List.list.count; q++) {
      qos_flow_setup_t *qosflowSetup = &drbSetup->qosFlows[q];
      E1AP_QoS_Flow_Item_t *in_qosflowSetup = drb->flow_Setup_List.list.array[q];
      qosflowSetup->qfi = in_qosflowSetup->qoS_Flow_Identifier;
    }
  }
  return true;
}

/**
 * @brief Encode an item of the E1 PDU Session Resource Setup List (9.3.3.5 of 3GPP TS 38.463)
 */
static bool e1_encode_pdu_session_setup_item(E1AP_PDU_Session_Resource_Setup_Item_t *item, const pdu_session_setup_t *in)
{
  // PDU Session ID (M)
  item->pDU_Session_ID = in->id;
  // NG DL UP Transport Layer Information (M)
  item->nG_DL_UP_TNL_Information.present = E1AP_UP_TNL_Information_PR_gTPTunnel;
  asn1cCalloc(item->nG_DL_UP_TNL_Information.choice.gTPTunnel, gTPTunnel);
  TRANSPORT_LAYER_ADDRESS_IPv4_TO_BIT_STRING(in->tlAddress, &gTPTunnel->transportLayerAddress);
  INT32_TO_OCTET_STRING(in->teId, &gTPTunnel->gTP_TEID);
  // DRB Setup List (1..<maxnoofDRBs>)
  for (const DRB_nGRAN_setup_t *j = in->DRBnGRanList; j < in->DRBnGRanList + in->numDRBSetup; j++) {
    asn1cSequenceAdd(item->dRB_Setup_List_NG_RAN.list, E1AP_DRB_Setup_Item_NG_RAN_t, ieC3_1_1);
    // DRB ID (M)
    ieC3_1_1->dRB_ID = j->id;
    // UL UP Parameters (M)
    for (const up_params_t *k = j->UpParamList; k < j->UpParamList + j->numUpParam; k++) {
      asn1cSequenceAdd(ieC3_1_1->uL_UP_Transport_Parameters.list, E1AP_UP_Parameters_Item_t, ieC3_1_1_1);
      ieC3_1_1_1->uP_TNL_Information.present = E1AP_UP_TNL_Information_PR_gTPTunnel;
      asn1cCalloc(ieC3_1_1_1->uP_TNL_Information.choice.gTPTunnel, gTPTunnel);
      TRANSPORT_LAYER_ADDRESS_IPv4_TO_BIT_STRING(k->tlAddress, &gTPTunnel->transportLayerAddress);
      INT32_TO_OCTET_STRING(k->teId, &gTPTunnel->gTP_TEID);
    }
    // Flow Setup List(M)
    for (const qos_flow_setup_t *k = j->qosFlows; k < j->qosFlows + j->numQosFlowSetup; k++) {
      asn1cSequenceAdd(ieC3_1_1->flow_Setup_List.list, E1AP_QoS_Flow_Item_t, ieC3_1_1_1);
      ieC3_1_1_1->qoS_Flow_Identifier = k->qfi;
    }
  }
  // DRB Failed List (O)
  if (in->numDRBFailed > 0)
    item->dRB_Failed_List_NG_RAN = calloc_or_fail(1, sizeof(*item->dRB_Failed_List_NG_RAN));
  // DRB Failed Item
  for (const DRB_nGRAN_failed_t *j = in->DRBnGRanFailedList; j < in->DRBnGRanFailedList + in->numDRBFailed; j++) {
    asn1cSequenceAdd(item->dRB_Failed_List_NG_RAN->list, E1AP_DRB_Failed_Item_NG_RAN_t, ieC3_1_1);
    // DRB ID (M)
    ieC3_1_1->dRB_ID = j->id;
    // Cause (M)
    ieC3_1_1->cause.present = j->cause_type;
    switch (ieC3_1_1->cause.present) {
      case E1AP_Cause_PR_radioNetwork:
        ieC3_1_1->cause.choice.radioNetwork = j->cause;
        break;
      case E1AP_Cause_PR_transport:
        ieC3_1_1->cause.choice.transport = j->cause;
        break;
      case E1AP_Cause_PR_protocol:
        ieC3_1_1->cause.choice.protocol = j->cause;
        break;
      case E1AP_Cause_PR_misc:
        ieC3_1_1->cause.choice.misc = j->cause;
        break;
      default:
        PRINT_ERROR("DRB setup failure cause out of expected range\n");
        break;
    }
  }
  return true;
}

/**
 * @brief Encode E1 Bearer Context Setup Response (9.2.2.2 of 3GPP TS 38.463)
 */
E1AP_E1AP_PDU_t *encode_E1_bearer_context_setup_response(const e1ap_bearer_setup_resp_t *in)
{
  E1AP_E1AP_PDU_t *pdu = calloc_or_fail(1, sizeof(*pdu));
  // PDU Type
  pdu->present = E1AP_E1AP_PDU_PR_successfulOutcome;
  asn1cCalloc(pdu->choice.successfulOutcome, msg);
  msg->procedureCode = E1AP_ProcedureCode_id_bearerContextSetup;
  msg->criticality = E1AP_Criticality_reject;
  msg->value.present = E1AP_SuccessfulOutcome__value_PR_BearerContextSetupResponse;
  E1AP_BearerContextSetupResponse_t *out = &pdu->choice.successfulOutcome->value.choice.BearerContextSetupResponse;
  // gNB-CU-CP UE E1AP ID (M)
  asn1cSequenceAdd(out->protocolIEs.list, E1AP_BearerContextSetupResponseIEs_t, ieC1);
  ieC1->id = E1AP_ProtocolIE_ID_id_gNB_CU_CP_UE_E1AP_ID;
  ieC1->criticality = E1AP_Criticality_reject;
  ieC1->value.present = E1AP_BearerContextSetupResponseIEs__value_PR_GNB_CU_CP_UE_E1AP_ID;
  ieC1->value.choice.GNB_CU_CP_UE_E1AP_ID = in->gNB_cu_cp_ue_id;
  // gNB-CU-UP UE E1AP ID (M)
  asn1cSequenceAdd(out->protocolIEs.list, E1AP_BearerContextSetupResponseIEs_t, ieC2);
  ieC2->id = E1AP_ProtocolIE_ID_id_gNB_CU_UP_UE_E1AP_ID;
  ieC2->criticality = E1AP_Criticality_reject;
  ieC2->value.present = E1AP_BearerContextSetupResponseIEs__value_PR_GNB_CU_UP_UE_E1AP_ID;
  ieC2->value.choice.GNB_CU_CP_UE_E1AP_ID = in->gNB_cu_up_ue_id;
  // Message Type (M)
  asn1cSequenceAdd(out->protocolIEs.list, E1AP_BearerContextSetupResponseIEs_t, ieC3);
  ieC3->id = E1AP_ProtocolIE_ID_id_System_BearerContextSetupResponse;
  ieC3->criticality = E1AP_Criticality_reject;
  ieC3->value.present = E1AP_BearerContextSetupResponseIEs__value_PR_System_BearerContextSetupResponse;
  // CHOICE System: default NG-RAN (M)
  ieC3->value.choice.System_BearerContextSetupResponse.present =
      E1AP_System_BearerContextSetupResponse_PR_nG_RAN_BearerContextSetupResponse;
  E1AP_ProtocolIE_Container_4932P22_t *msgNGRAN_list = calloc_or_fail(1, sizeof(*msgNGRAN_list));
  ieC3->value.choice.System_BearerContextSetupResponse.choice.nG_RAN_BearerContextSetupResponse =
      (struct E1AP_ProtocolIE_Container *)msgNGRAN_list;
  asn1cSequenceAdd(msgNGRAN_list->list, E1AP_NG_RAN_BearerContextSetupResponse_t, msgNGRAN);
  msgNGRAN->id = E1AP_ProtocolIE_ID_id_PDU_Session_Resource_Setup_List;
  msgNGRAN->criticality = E1AP_Criticality_reject;
  msgNGRAN->value.present = E1AP_NG_RAN_BearerContextSetupResponse__value_PR_PDU_Session_Resource_Setup_List;
  E1AP_PDU_Session_Resource_Setup_List_t *pduSetup = &msgNGRAN->value.choice.PDU_Session_Resource_Setup_List;
  for (const pdu_session_setup_t *i = in->pduSession; i < in->pduSession + in->numPDUSessions; i++) {
    asn1cSequenceAdd(pduSetup->list, E1AP_PDU_Session_Resource_Setup_Item_t, ieC3_1);
    e1_encode_pdu_session_setup_item(ieC3_1, i);
  }
  return pdu;
}

/**
 * @brief Decode E1 Bearer Context Setup Response (9.2.2.2 of 3GPP TS 38.463)
 */
bool decode_E1_bearer_context_setup_response(const E1AP_E1AP_PDU_t *pdu, e1ap_bearer_setup_resp_t *out)
{
  // Check message type
  _E1_EQ_CHECK_INT(pdu->present, E1AP_E1AP_PDU_PR_successfulOutcome);
  _E1_EQ_CHECK_INT(pdu->choice.successfulOutcome->procedureCode, E1AP_ProcedureCode_id_bearerContextSetup);
  _E1_EQ_CHECK_INT(pdu->choice.successfulOutcome->criticality, E1AP_Criticality_reject);
  _E1_EQ_CHECK_INT(pdu->choice.successfulOutcome->value.present, E1AP_SuccessfulOutcome__value_PR_BearerContextSetupResponse);

  const E1AP_BearerContextSetupResponse_t *in = &pdu->choice.successfulOutcome->value.choice.BearerContextSetupResponse;
  E1AP_BearerContextSetupResponseIEs_t *ie;
  // Check mandatory IEs first
  E1AP_LIB_FIND_IE(E1AP_BearerContextSetupResponseIEs_t, ie, in, E1AP_ProtocolIE_ID_id_gNB_CU_CP_UE_E1AP_ID, true);
  E1AP_LIB_FIND_IE(E1AP_BearerContextSetupResponseIEs_t, ie, in, E1AP_ProtocolIE_ID_id_gNB_CU_UP_UE_E1AP_ID, true);
  E1AP_LIB_FIND_IE(E1AP_BearerContextSetupResponseIEs_t, ie, in, E1AP_ProtocolIE_ID_id_System_BearerContextSetupResponse, true);
  // Loop over all IEs
  for (int i = 0; i < in->protocolIEs.list.count; i++) {
    ie = in->protocolIEs.list.array[i];

    switch (ie->id) {
      case E1AP_ProtocolIE_ID_id_gNB_CU_CP_UE_E1AP_ID:
        _E1_EQ_CHECK_INT(ie->value.present, E1AP_BearerContextSetupResponseIEs__value_PR_GNB_CU_CP_UE_E1AP_ID);
        out->gNB_cu_cp_ue_id = ie->value.choice.GNB_CU_CP_UE_E1AP_ID;
        break;

      case E1AP_ProtocolIE_ID_id_gNB_CU_UP_UE_E1AP_ID:
        _E1_EQ_CHECK_INT(ie->value.present, E1AP_BearerContextSetupResponseIEs__value_PR_GNB_CU_UP_UE_E1AP_ID);
        out->gNB_cu_up_ue_id = ie->value.choice.GNB_CU_UP_UE_E1AP_ID;
        break;

      case E1AP_ProtocolIE_ID_id_System_BearerContextSetupResponse:
        _E1_EQ_CHECK_INT(ie->value.present, E1AP_BearerContextSetupResponseIEs__value_PR_System_BearerContextSetupResponse);
        switch (ie->value.choice.System_BearerContextSetupResponse.present) {
          case E1AP_System_BearerContextSetupResponse_PR_nG_RAN_BearerContextSetupResponse: {
            E1AP_ProtocolIE_Container_4932P22_t *msgNGRAN_list =
                (E1AP_ProtocolIE_Container_4932P22_t *)
                    ie->value.choice.System_BearerContextSetupResponse.choice.nG_RAN_BearerContextSetupResponse;
            _E1_EQ_CHECK_INT(msgNGRAN_list->list.count, 1);
            E1AP_NG_RAN_BearerContextSetupResponse_t *msgNGRAN = msgNGRAN_list->list.array[0];
            _E1_EQ_CHECK_INT(msgNGRAN->id, E1AP_ProtocolIE_ID_id_PDU_Session_Resource_Setup_List);
            _E1_EQ_CHECK_INT(msgNGRAN->value.present,
                             E1AP_NG_RAN_BearerContextSetupResponse__value_PR_PDU_Session_Resource_Setup_List);
            E1AP_PDU_Session_Resource_Setup_List_t *pduSetupList = &msgNGRAN->value.choice.PDU_Session_Resource_Setup_List;
            out->numPDUSessions = pduSetupList->list.count;
            for (int i = 0; i < pduSetupList->list.count; i++) {
              pdu_session_setup_t *pduSetup = out->pduSession + i;
              E1AP_PDU_Session_Resource_Setup_Item_t *pdu_session = pduSetupList->list.array[i];
              e1_decode_pdu_session_setup_item(pduSetup, pdu_session);
            }
          } break;

          case E1AP_System_BearerContextSetupResponse_PR_e_UTRAN_BearerContextSetupResponse:
            PRINT_ERROR("DRB Setup List E-UTRAN not supported in E1 Setup Response\n");
            return false;
            break;

          default:
            PRINT_ERROR("Unknown DRB Setup List in E1 Setup Response\n");
            return false;
            break;
        }
        break;

      default:
        PRINT_ERROR("Handle for this IE is not implemented (or) invalid IE detected\n");
        return false;
        break;
    }
  }
  return true;
}

/**
 * @brief Deep copy function for E1 BEARER CONTEXT SETUP RESPONSE
 */
e1ap_bearer_setup_resp_t cp_bearer_context_setup_response(const e1ap_bearer_setup_resp_t *msg)
{
  e1ap_bearer_setup_resp_t cp = {0};
  // Copy basic fields
  cp = *msg;
  // Copy PDU Sessions Setup
  for (int i = 0; i < msg->numPDUSessions; i++) {
    cp.pduSession[i] = msg->pduSession[i];
    // Copy DRB Setup
    for (int j = 0; j < msg->pduSession[i].numDRBSetup; j++) {
      cp.pduSession[i].DRBnGRanList[j] = msg->pduSession[i].DRBnGRanList[j];
      for (int k = 0; k < msg->pduSession[i].DRBnGRanList[j].numQosFlowSetup; k++) {
        cp.pduSession[i].DRBnGRanList[j].qosFlows[k] = msg->pduSession[i].DRBnGRanList[j].qosFlows[k];
      }
    }
    cp.pduSession[i].numDRBFailed = msg->pduSession[i].numDRBFailed;
    // Copy DRB Failed
    for (int j = 0; j < msg->pduSession[i].numDRBFailed; j++) {
      cp.pduSession[i].DRBnGRanFailedList[j] = msg->pduSession[i].DRBnGRanFailedList[j];
    }
  }
  return cp;
}

/**
 * @brief E1AP Bearer Context Setup Response equality check
 */
bool eq_bearer_context_setup_response(const e1ap_bearer_setup_resp_t *a, const e1ap_bearer_setup_resp_t *b)
{
  // Check basic members
  _E1_EQ_CHECK_INT(a->gNB_cu_cp_ue_id, b->gNB_cu_cp_ue_id);
  _E1_EQ_CHECK_INT(a->gNB_cu_up_ue_id, b->gNB_cu_up_ue_id);
  _E1_EQ_CHECK_INT(a->numPDUSessions, b->numPDUSessions);
  // Check PDU Sessions Setup
  if (a->numPDUSessions != b->numPDUSessions)
    return false;
  for (int i = 0; i < a->numPDUSessions; i++) {
    const pdu_session_setup_t *ps_a = &a->pduSession[i];
    const pdu_session_setup_t *ps_b = &b->pduSession[i];
    _E1_EQ_CHECK_LONG(ps_a->id, ps_b->id);
    _E1_EQ_CHECK_INT(ps_a->tlAddress, ps_b->tlAddress);
    _E1_EQ_CHECK_LONG(ps_a->teId, ps_b->teId);
    _E1_EQ_CHECK_INT(ps_a->numDRBSetup, ps_b->numDRBSetup);
    _E1_EQ_CHECK_INT(ps_a->numDRBFailed, ps_b->numDRBFailed);
    // Check DRB Setup
    for (int j = 0; j < ps_a->numDRBSetup; j++) {
      const DRB_nGRAN_setup_t *drb_a = &ps_a->DRBnGRanList[j];
      const DRB_nGRAN_setup_t *drb_b = &ps_b->DRBnGRanList[j];
      _E1_EQ_CHECK_LONG(drb_a->id, drb_b->id);
      _E1_EQ_CHECK_INT(drb_a->numUpParam, drb_b->numUpParam);
      _E1_EQ_CHECK_INT(drb_a->numQosFlowSetup, drb_b->numQosFlowSetup);
    }
    // Check DRB Failed
    for (int j = 0; j < ps_a->numDRBFailed; j++) {
      const DRB_nGRAN_failed_t *drbf_a = &ps_a->DRBnGRanFailedList[j];
      const DRB_nGRAN_failed_t *drbf_b = &ps_b->DRBnGRanFailedList[j];
      _E1_EQ_CHECK_LONG(drbf_a->id, drbf_b->id);
      _E1_EQ_CHECK_LONG(drbf_a->cause_type, drbf_b->cause_type);
      _E1_EQ_CHECK_LONG(drbf_a->cause, drbf_b->cause);
    }
  }
  return true;
}

/**
 * @brief E1AP Bearer Context Setup Response memory management
 */
void free_e1ap_context_setup_response(const e1ap_bearer_setup_resp_t *msg)
{
  // Do nothing (no dynamic allocation)
}
