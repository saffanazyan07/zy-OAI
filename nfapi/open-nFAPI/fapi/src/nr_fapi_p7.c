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
#include "nr_fapi_p7.h"
#include "nr_nfapi_p7.h"
#include "debug.h"

uint8_t fapi_nr_p7_message_body_pack(nfapi_nr_p7_message_header_t *header,
                                     uint8_t **ppWritePackedMsg,
                                     uint8_t *end,
                                     nfapi_p7_codec_config_t *config)
{
  // look for the specific message
  uint8_t result = 0;
  switch (header->message_id) {
    case NFAPI_NR_PHY_MSG_TYPE_DL_TTI_REQUEST:
      result = pack_dl_tti_request(header, ppWritePackedMsg, end, config);
    break;

    default: {
      if (header->message_id >= NFAPI_VENDOR_EXT_MSG_MIN && header->message_id <= NFAPI_VENDOR_EXT_MSG_MAX) {
        if (config && config->pack_p7_vendor_extension) {
          result = (config->pack_p7_vendor_extension)(header, ppWritePackedMsg, end, config);
        } else {
          NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s VE NFAPI message ID %d. No ve decoder provided\n", __FUNCTION__, header->message_id);
        }
      } else {
        NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s NFAPI Unknown message ID %d\n", __FUNCTION__, header->message_id);
      }
    } break;
  }

  if (result == 0) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "P7 Pack failed to pack message\n");
  }
  return result;
}

int fapi_nr_p7_message_pack(void *pMessageBuf, void *pPackedBuf, uint32_t packedBufLen, nfapi_p7_codec_config_t *config)
{
  if (pMessageBuf == NULL || pPackedBuf == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "P7 Pack supplied pointers are null\n");
    return -1;
  }

  nfapi_nr_p7_message_header_t *pMessageHeader = pMessageBuf;
  uint8_t *pWritePackedMessage = pPackedBuf;
  uint8_t *pPackMessageEnd = pPackedBuf + packedBufLen;
  uint8_t *pPackedLengthField = &pWritePackedMessage[4];
  uint8_t *pPacketBodyField = &pWritePackedMessage[8];
  uint8_t *pPacketBodyFieldStart = &pWritePackedMessage[8];

  const uint8_t result = fapi_nr_p7_message_body_pack(pMessageHeader, &pPacketBodyField, pPackMessageEnd, config);
  AssertFatal(result > 0, "fapi_nr_p7_message_body_pack error packing message body %d\n", result);

  // PHY API message header
  // Number of messages [0]
  // Opaque handle [1]
  // PHY API Message structure
  // Message type ID [2,3]
  // Message Length [4,5,6,7]
  // Message Body [8,...]
  if (!(push8(1, &pWritePackedMessage, pPackMessageEnd) && push8(0, &pWritePackedMessage, pPackMessageEnd)
        && push16(pMessageHeader->message_id, &pWritePackedMessage, pPackMessageEnd))) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "P7 Pack header failed\n");
    return -1;
  }

  // check for a valid message length
  uintptr_t msgHead = (uintptr_t)pPacketBodyFieldStart;
  uintptr_t msgEnd = (uintptr_t)pPacketBodyField;
  uint32_t packedMsgLen = msgEnd - msgHead;
  if (packedMsgLen > packedBufLen) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "Packed message length error %d, buffer supplied %d\n", packedMsgLen, packedBufLen);
    return -1;
  }

  // Update the message length in the header
  pMessageHeader->message_length = packedMsgLen;

  // Update the message length in the header
  if (!push32(packedMsgLen, &pPackedLengthField, pPackMessageEnd))
    return -1;

  return packedMsgLen;
}

int fapi_nr_p7_message_unpack(void *pMessageBuf,
                              uint32_t messageBufLen,
                              void *pUnpackedBuf,
                              uint32_t unpackedBufLen,
                              nfapi_p7_codec_config_t *config)
{
  int result = 0;
  nfapi_nr_p7_message_header_t *pMessageHeader = (nfapi_nr_p7_message_header_t *)pUnpackedBuf;
  fapi_message_header_t fapi_hdr;
  uint8_t *pReadPackedMessage = pMessageBuf;

  AssertFatal(pMessageBuf != NULL && pUnpackedBuf != NULL, "P7 unpack supplied pointers are null");
  uint8_t *end = (uint8_t *)pMessageBuf + messageBufLen;
  AssertFatal(messageBufLen >= NFAPI_HEADER_LENGTH && unpackedBufLen >= sizeof(fapi_message_header_t),
              "P5 unpack supplied message buffer is too small %d, %d\n",
              messageBufLen,
              unpackedBufLen);


  if (fapi_nr_message_header_unpack(&pReadPackedMessage, NFAPI_HEADER_LENGTH, &fapi_hdr, sizeof(fapi_message_header_t), 0) < 0) {
    // failed to read the header
    return -1;
  }
  pMessageHeader->message_length = fapi_hdr.message_length;
  pMessageHeader->message_id = fapi_hdr.message_id;
  if ((uint8_t *)(pMessageBuf + pMessageHeader->message_length) > end) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "P7 unpack message length is greater than the message buffer \n");
    return -1;
  }

  // look for the specific message
  switch (pMessageHeader->message_id) {
    case NFAPI_NR_PHY_MSG_TYPE_DL_TTI_REQUEST:
      if (check_nr_fapi_unpack_length(NFAPI_NR_PHY_MSG_TYPE_DL_TTI_REQUEST, unpackedBufLen))
        result = unpack_dl_tti_request(&pReadPackedMessage, end, pMessageHeader, config);
    break;

    default:

      if (pMessageHeader->message_id >= NFAPI_VENDOR_EXT_MSG_MIN && pMessageHeader->message_id <= NFAPI_VENDOR_EXT_MSG_MAX) {
        if (config && config->unpack_p7_vendor_extension) {
          result = (config->unpack_p7_vendor_extension)(pMessageHeader, &pReadPackedMessage, end, config);
        } else {
          NFAPI_TRACE(NFAPI_TRACE_ERROR,
                      "%s VE NFAPI message ID %d. No ve decoder provided\n",
                      __FUNCTION__,
                      pMessageHeader->message_id);
        }
      } else {
        NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s NFAPI Unknown message ID %d\n", __FUNCTION__, pMessageHeader->message_id);
      }
      break;
  }

  if (result == 0) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "P7 Unpack failed to unpack message\n");
    return -1;
  }
  return 0;
}

static uint8_t pack_nr_tx_beamforming_pdu(const nfapi_nr_tx_precoding_and_beamforming_t *beamforming_pdu,
                                          uint8_t **ppWritePackedMsg,
                                          uint8_t *end)
{ // Pack RX Beamforming PDU
  if (!(push16(beamforming_pdu->num_prgs, ppWritePackedMsg, end) && push16(beamforming_pdu->prg_size, ppWritePackedMsg, end)
        && push8(beamforming_pdu->dig_bf_interfaces, ppWritePackedMsg, end))) {
    return 0;
  }
  for (int prg = 0; prg < beamforming_pdu->num_prgs; prg++) {
    if (!push16(beamforming_pdu->prgs_list[prg].pm_idx, ppWritePackedMsg, end)) {
      return 0;
    }
    for (int digBFInterface = 0; digBFInterface < beamforming_pdu->dig_bf_interfaces; digBFInterface++) {
      if (!push16(beamforming_pdu->prgs_list[prg].dig_bf_interface_list[digBFInterface].beam_idx, ppWritePackedMsg, end)) {
        return 0;
      }
    }
  }
  return 1;
}

static uint8_t pack_dl_tti_csi_rs_pdu_rel15_value(void *tlv, uint8_t **ppWritePackedMsg, uint8_t *end)
{
  nfapi_nr_dl_tti_csi_rs_pdu_rel15_t *value = (nfapi_nr_dl_tti_csi_rs_pdu_rel15_t *)tlv;
  if (!(push16(value->bwp_size, ppWritePackedMsg, end) && push16(value->bwp_start, ppWritePackedMsg, end)
        && push8(value->subcarrier_spacing, ppWritePackedMsg, end) && push8(value->cyclic_prefix, ppWritePackedMsg, end)
        && push16(value->start_rb, ppWritePackedMsg, end) && push16(value->nr_of_rbs, ppWritePackedMsg, end)
        && push8(value->csi_type, ppWritePackedMsg, end) && push8(value->row, ppWritePackedMsg, end)
        && push16(value->freq_domain, ppWritePackedMsg, end) && push8(value->symb_l0, ppWritePackedMsg, end)
        && push8(value->symb_l1, ppWritePackedMsg, end) && push8(value->cdm_type, ppWritePackedMsg, end)
        && push8(value->freq_density, ppWritePackedMsg, end) && push16(value->scramb_id, ppWritePackedMsg, end)
        && push8(value->power_control_offset, ppWritePackedMsg, end)
        && push8(value->power_control_offset_ss, ppWritePackedMsg, end))) {
    return 0;
  }

  // Precoding and beamforming
  if(!pack_nr_tx_beamforming_pdu(&value->precodingAndBeamforming,ppWritePackedMsg, end)) {
    return 0;
  }
  return 1;
}

static uint8_t pack_dl_tti_pdcch_pdu_rel15_value(void *tlv, uint8_t **ppWritePackedMsg, uint8_t *end)
{
  nfapi_nr_dl_tti_pdcch_pdu_rel15_t *value = (nfapi_nr_dl_tti_pdcch_pdu_rel15_t *)tlv;

  if (!(push16(value->BWPSize, ppWritePackedMsg, end) && push16(value->BWPStart, ppWritePackedMsg, end)
        && push8(value->SubcarrierSpacing, ppWritePackedMsg, end) && push8(value->CyclicPrefix, ppWritePackedMsg, end)
        && push8(value->StartSymbolIndex, ppWritePackedMsg, end) && push8(value->DurationSymbols, ppWritePackedMsg, end)
        && pusharray8(value->FreqDomainResource, 6, 6, ppWritePackedMsg, end)
        && push8(value->CceRegMappingType, ppWritePackedMsg, end) && push8(value->RegBundleSize, ppWritePackedMsg, end)
        && push8(value->InterleaverSize, ppWritePackedMsg, end) && push8(value->CoreSetType, ppWritePackedMsg, end)
        && push16(value->ShiftIndex, ppWritePackedMsg, end) && push8(value->precoderGranularity, ppWritePackedMsg, end)
        && push16(value->numDlDci, ppWritePackedMsg, end))) {
    return 0;
  }

  for (uint16_t i = 0; i < value->numDlDci; ++i) {
    if (!(push16(value->dci_pdu[i].RNTI, ppWritePackedMsg, end) && push16(value->dci_pdu[i].ScramblingId, ppWritePackedMsg, end)
          && push16(value->dci_pdu[i].ScramblingRNTI, ppWritePackedMsg, end)
          && push8(value->dci_pdu[i].CceIndex, ppWritePackedMsg, end)
          && push8(value->dci_pdu[i].AggregationLevel, ppWritePackedMsg, end))) {
      return 0;
    }
    // Precoding and beamforming
    if(!pack_nr_tx_beamforming_pdu(&value->dci_pdu[i].precodingAndBeamforming,ppWritePackedMsg, end)) {
      return 0;
    }
    // TX Power info
    if (!(push8(value->dci_pdu[i].beta_PDCCH_1_0, ppWritePackedMsg, end)
          && push8(value->dci_pdu[i].powerControlOffsetSS, ppWritePackedMsg, end) &&
          // DCI Payload fields
          push16(value->dci_pdu[i].PayloadSizeBits, ppWritePackedMsg, end) &&
          // Pack DCI Payload
          pack_dci_payload(value->dci_pdu[i].Payload, value->dci_pdu[i].PayloadSizeBits, ppWritePackedMsg, end))) {
      return 0;
    }
  }
  return 1;
}

static uint8_t pack_dl_tti_pdsch_pdu_rel15_value(void *tlv, uint8_t **ppWritePackedMsg, uint8_t *end)
{
  nfapi_nr_dl_tti_pdsch_pdu_rel15_t *value = (nfapi_nr_dl_tti_pdsch_pdu_rel15_t *)tlv;

  if (!(push16(value->pduBitmap, ppWritePackedMsg, end) && push16(value->rnti, ppWritePackedMsg, end)
        && push16(value->pduIndex, ppWritePackedMsg, end) && push16(value->BWPSize, ppWritePackedMsg, end)
        && push16(value->BWPStart, ppWritePackedMsg, end) && push8(value->SubcarrierSpacing, ppWritePackedMsg, end)
        && push8(value->CyclicPrefix, ppWritePackedMsg, end) && push8(value->NrOfCodewords, ppWritePackedMsg, end))) {
    return 0;
  }
  for (int i = 0; i < value->NrOfCodewords; ++i) {
    if (!(push16(value->targetCodeRate[i], ppWritePackedMsg, end) && push8(value->qamModOrder[i], ppWritePackedMsg, end)
          && push8(value->mcsIndex[i], ppWritePackedMsg, end) && push8(value->mcsTable[i], ppWritePackedMsg, end)
          && push8(value->rvIndex[i], ppWritePackedMsg, end) && push32(value->TBSize[i], ppWritePackedMsg, end))) {
      return 0;
    }
  }

  if (!(push16(value->dataScramblingId, ppWritePackedMsg, end) && push8(value->nrOfLayers, ppWritePackedMsg, end)
        && push8(value->transmissionScheme, ppWritePackedMsg, end) && push8(value->refPoint, ppWritePackedMsg, end)
        && push16(value->dlDmrsSymbPos, ppWritePackedMsg, end) && push8(value->dmrsConfigType, ppWritePackedMsg, end)
        && push16(value->dlDmrsScramblingId, ppWritePackedMsg, end) && push8(value->SCID, ppWritePackedMsg, end)
        && push8(value->numDmrsCdmGrpsNoData, ppWritePackedMsg, end) && push16(value->dmrsPorts, ppWritePackedMsg, end)
        && push8(value->resourceAlloc, ppWritePackedMsg, end) && (int)pusharray8(value->rbBitmap, 36, 36, ppWritePackedMsg, end)
        && push16(value->rbStart, ppWritePackedMsg, end) && push16(value->rbSize, ppWritePackedMsg, end)
        && push8(value->VRBtoPRBMapping, ppWritePackedMsg, end) && push8(value->StartSymbolIndex, ppWritePackedMsg, end)
        && push8(value->NrOfSymbols, ppWritePackedMsg, end))) {
    return 0;
  }

  // Check pduBitMap bit 0 to add or not PTRS parameters
  if (value->pduBitmap & 0b1) {
    if (!(push8(value->PTRSPortIndex, ppWritePackedMsg, end) && push8(value->PTRSTimeDensity, ppWritePackedMsg, end)
          && push8(value->PTRSFreqDensity, ppWritePackedMsg, end) && push8(value->PTRSReOffset, ppWritePackedMsg, end)
          && push8(value->nEpreRatioOfPDSCHToPTRS, ppWritePackedMsg, end))) {
      return 0;
    }
  }

  // Precoding and beamforming
  if(!pack_nr_tx_beamforming_pdu(&value->precodingAndBeamforming,ppWritePackedMsg, end)) {
    return 0;
  }

  if (!(push8(value->powerControlOffset, ppWritePackedMsg, end) && push8(value->powerControlOffsetSS, ppWritePackedMsg, end))) {
    return 0;
  }

  // Check pduBitMap bit 1 to add or not CBG parameters
  if (value->pduBitmap & 0b10) {
    if (!(push8(value->isLastCbPresent, ppWritePackedMsg, end) && push8(value->isInlineTbCrc, ppWritePackedMsg, end)
          && push32(value->dlTbCrc, ppWritePackedMsg, end))) {
      return 0;
    }
  }

  if (!push8(value->maintenance_parms_v3.ldpcBaseGraph, ppWritePackedMsg, end)
      || !push32(value->maintenance_parms_v3.tbSizeLbrmBytes, ppWritePackedMsg, end))
    return 0;

  return 1;
}

static uint8_t pack_dl_tti_ssb_pdu_rel15_value(void *tlv, uint8_t **ppWritePackedMsg, uint8_t *end)
{
  NFAPI_TRACE(NFAPI_TRACE_DEBUG, "Packing ssb. \n");
  nfapi_nr_dl_tti_ssb_pdu_rel15_t *value = (nfapi_nr_dl_tti_ssb_pdu_rel15_t *)tlv;

  if (!(push16(value->PhysCellId, ppWritePackedMsg, end) && push8(value->BetaPss, ppWritePackedMsg, end)
        && push8(value->SsbBlockIndex, ppWritePackedMsg, end) && push8(value->SsbSubcarrierOffset, ppWritePackedMsg, end)
        && push16(value->ssbOffsetPointA, ppWritePackedMsg, end) && push8(value->bchPayloadFlag, ppWritePackedMsg, end)
        && push8((value->bchPayload >> 16) & 0xff, ppWritePackedMsg, end)
        && push8((value->bchPayload >> 8) & 0xff, ppWritePackedMsg, end) && push8(value->bchPayload & 0xff, ppWritePackedMsg, end)
        && push8(0, ppWritePackedMsg, end))) {
    return 0;
  }
  // Precoding and beamforming
  if(!pack_nr_tx_beamforming_pdu(&value->precoding_and_beamforming,ppWritePackedMsg, end)) {
    return 0;
  }
  return 1;
}

static uint8_t pack_dl_tti_request_body_value(void *tlv, uint8_t **ppWritePackedMsg, uint8_t *end)
{
  nfapi_nr_dl_tti_request_pdu_t *value = (nfapi_nr_dl_tti_request_pdu_t *)tlv;
  uintptr_t msgHead = (uintptr_t)*ppWritePackedMsg;
  if (!push16(value->PDUType, ppWritePackedMsg, end))
    return 0;
  uint8_t *pPackedLengthField = *ppWritePackedMsg;
  if (!push16(value->PDUSize, ppWritePackedMsg, end))
    return 0;

  // first match the pdu type, then call the respective function
  switch (value->PDUType) {
    case NFAPI_NR_DL_TTI_PDCCH_PDU_TYPE: {
      if (!(pack_dl_tti_pdcch_pdu_rel15_value(&value->pdcch_pdu.pdcch_pdu_rel15, ppWritePackedMsg, end)))
        return 0;
    } break;

    case NFAPI_NR_DL_TTI_PDSCH_PDU_TYPE: {
      if (!(pack_dl_tti_pdsch_pdu_rel15_value(&value->pdsch_pdu.pdsch_pdu_rel15, ppWritePackedMsg, end)))
        return 0;
    } break;

    case NFAPI_NR_DL_TTI_CSI_RS_PDU_TYPE: {
      if (!(pack_dl_tti_csi_rs_pdu_rel15_value(&value->csi_rs_pdu.csi_rs_pdu_rel15, ppWritePackedMsg, end)))
        return 0;
    } break;

    case NFAPI_NR_DL_TTI_SSB_PDU_TYPE: {
      if (!(pack_dl_tti_ssb_pdu_rel15_value(&value->ssb_pdu.ssb_pdu_rel15, ppWritePackedMsg, end)))
        return 0;
    } break;

    default: {
      NFAPI_TRACE(NFAPI_TRACE_ERROR, "FIXME : Invalid DL_TTI pdu type %d \n", value->PDUType);
    } break;
  }
  // pack proper size
  uintptr_t msgEnd = (uintptr_t)*ppWritePackedMsg;
  uint16_t packedMsgLen = msgEnd - msgHead;
  value->PDUSize = packedMsgLen;
  return push16(value->PDUSize, &pPackedLengthField, end);
}

uint8_t pack_dl_tti_request(void *msg, uint8_t **ppWritePackedMsg, uint8_t *end, nfapi_p7_codec_config_t *config)
{
  nfapi_nr_dl_tti_request_t *pNfapiMsg = (nfapi_nr_dl_tti_request_t *)msg;

  if (!(push16(pNfapiMsg->SFN, ppWritePackedMsg, end) && push16(pNfapiMsg->Slot, ppWritePackedMsg, end)
        && push8(pNfapiMsg->dl_tti_request_body.nPDUs, ppWritePackedMsg, end)
        && push8(pNfapiMsg->dl_tti_request_body.nGroup, ppWritePackedMsg, end))) {
    return 0;
  }
  for (int i = 0; i < pNfapiMsg->dl_tti_request_body.nPDUs; i++) {
    if (!pack_dl_tti_request_body_value(&pNfapiMsg->dl_tti_request_body.dl_tti_pdu_list[i], ppWritePackedMsg, end)) {
      return 0;
    }
  }

  for (int i = 0; i < pNfapiMsg->dl_tti_request_body.nGroup; i++) {
    if (!push8(pNfapiMsg->dl_tti_request_body.nUe[i], ppWritePackedMsg, end))
      return 0;
    for (int j = 0; j < pNfapiMsg->dl_tti_request_body.nUe[i]; j++) {
      if (!(push32(pNfapiMsg->dl_tti_request_body.PduIdx[i][j], ppWritePackedMsg, end))) {
        return 0;
      }
    }
  }
  return 1;
}

static uint8_t unpack_nr_tx_beamforming_pdu(nfapi_nr_tx_precoding_and_beamforming_t *beamforming_pdu, uint8_t **ppReadPackedMsg, uint8_t *end)
{ // Unpack RX Beamforming PDU
  if (!(pull16(ppReadPackedMsg, &beamforming_pdu->num_prgs, end) && pull16(ppReadPackedMsg, &beamforming_pdu->prg_size, end)
        && pull8(ppReadPackedMsg, &beamforming_pdu->dig_bf_interfaces, end))) {
    return 0;
  }
  for (int prg = 0; prg < beamforming_pdu->num_prgs; prg++) {
    if (!(pull16(ppReadPackedMsg, &beamforming_pdu->prgs_list[prg].pm_idx, end))) {
      return 0;
    }
    for (int digBFInterface = 0; digBFInterface < beamforming_pdu->dig_bf_interfaces; digBFInterface++) {
      if (!pull16(ppReadPackedMsg, &beamforming_pdu->prgs_list[prg].dig_bf_interface_list[digBFInterface].beam_idx, end)) {
        return 0;
      }
    }
  }
  return 1;
}

static uint8_t unpack_dl_tti_pdcch_pdu_rel15_value(void *tlv, uint8_t **ppReadPackedMsg, uint8_t *end)
{
  nfapi_nr_dl_tti_pdcch_pdu_rel15_t *value = (nfapi_nr_dl_tti_pdcch_pdu_rel15_t *)tlv;

  if (!(pull16(ppReadPackedMsg, &value->BWPSize, end) && pull16(ppReadPackedMsg, &value->BWPStart, end)
        && pull8(ppReadPackedMsg, &value->SubcarrierSpacing, end) && pull8(ppReadPackedMsg, &value->CyclicPrefix, end)
        && pull8(ppReadPackedMsg, &value->StartSymbolIndex, end) && pull8(ppReadPackedMsg, &value->DurationSymbols, end)
        && pullarray8(ppReadPackedMsg, value->FreqDomainResource, 6, 6, end)
        && pull8(ppReadPackedMsg, &value->CceRegMappingType, end) && pull8(ppReadPackedMsg, &value->RegBundleSize, end)
        && pull8(ppReadPackedMsg, &value->InterleaverSize, end) && pull8(ppReadPackedMsg, &value->CoreSetType, end)
        && pull16(ppReadPackedMsg, &value->ShiftIndex, end) && pull8(ppReadPackedMsg, &value->precoderGranularity, end)
        && pull16(ppReadPackedMsg, &value->numDlDci, end))) {
    return 0;
  }

  for (uint16_t i = 0; i < value->numDlDci; ++i) {
    if (!(pull16(ppReadPackedMsg, &value->dci_pdu[i].RNTI, end) && pull16(ppReadPackedMsg, &value->dci_pdu[i].ScramblingId, end)
          && pull16(ppReadPackedMsg, &value->dci_pdu[i].ScramblingRNTI, end)
          && pull8(ppReadPackedMsg, &value->dci_pdu[i].CceIndex, end)
          && pull8(ppReadPackedMsg, &value->dci_pdu[i].AggregationLevel, end))) {
      return 0;
    }

    // Preocding and Beamforming
    if(!unpack_nr_tx_beamforming_pdu(&value->dci_pdu[i].precodingAndBeamforming, ppReadPackedMsg, end)) {
      return 0;
    }

    if (!(pull8(ppReadPackedMsg, &value->dci_pdu[i].beta_PDCCH_1_0, end)
          && pull8(ppReadPackedMsg, &value->dci_pdu[i].powerControlOffsetSS, end)
          && pull16(ppReadPackedMsg, &value->dci_pdu[i].PayloadSizeBits, end)
          && unpack_dci_payload(value->dci_pdu[i].Payload, value->dci_pdu[i].PayloadSizeBits, ppReadPackedMsg, end))) {
      return 0;
    }
  }
  return 1;
}

static uint8_t unpack_dl_tti_pdsch_pdu_rel15_value(void *tlv, uint8_t **ppReadPackedMsg, uint8_t *end)
{
  nfapi_nr_dl_tti_pdsch_pdu_rel15_t *value = (nfapi_nr_dl_tti_pdsch_pdu_rel15_t *)tlv;

  if (!(pull16(ppReadPackedMsg, &value->pduBitmap, end) && pull16(ppReadPackedMsg, &value->rnti, end)
        && pull16(ppReadPackedMsg, &value->pduIndex, end) && pull16(ppReadPackedMsg, &value->BWPSize, end)
        && pull16(ppReadPackedMsg, &value->BWPStart, end) && pull8(ppReadPackedMsg, &value->SubcarrierSpacing, end)
        && pull8(ppReadPackedMsg, &value->CyclicPrefix, end) && pull8(ppReadPackedMsg, &value->NrOfCodewords, end))) {
    return 0;
  }
  for (int i = 0; i < value->NrOfCodewords; ++i) {
    if (!(pull16(ppReadPackedMsg, &value->targetCodeRate[i], end) && pull8(ppReadPackedMsg, &value->qamModOrder[i], end)
          && pull8(ppReadPackedMsg, &value->mcsIndex[i], end) && pull8(ppReadPackedMsg, &value->mcsTable[i], end)
          && pull8(ppReadPackedMsg, &value->rvIndex[i], end) && pull32(ppReadPackedMsg, &value->TBSize[i], end))) {
      return 0;
    }
  }

  if (!(pull16(ppReadPackedMsg, &value->dataScramblingId, end) && pull8(ppReadPackedMsg, &value->nrOfLayers, end)
        && pull8(ppReadPackedMsg, &value->transmissionScheme, end) && pull8(ppReadPackedMsg, &value->refPoint, end)
        && pull16(ppReadPackedMsg, &value->dlDmrsSymbPos, end) && pull8(ppReadPackedMsg, &value->dmrsConfigType, end)
        && pull16(ppReadPackedMsg, &value->dlDmrsScramblingId, end) && pull8(ppReadPackedMsg, &value->SCID, end)
        && pull8(ppReadPackedMsg, &value->numDmrsCdmGrpsNoData, end) && pull16(ppReadPackedMsg, &value->dmrsPorts, end)
        && pull8(ppReadPackedMsg, &value->resourceAlloc, end) && pullarray8(ppReadPackedMsg, &value->rbBitmap[0], 36, 36, end)
        && pull16(ppReadPackedMsg, &value->rbStart, end) && pull16(ppReadPackedMsg, &value->rbSize, end)
        && pull8(ppReadPackedMsg, &value->VRBtoPRBMapping, end) && pull8(ppReadPackedMsg, &value->StartSymbolIndex, end)
        && pull8(ppReadPackedMsg, &value->NrOfSymbols, end))) {
    return 0;
  }
  // Check pduBitMap bit 0 to pull PTRS parameters or not
  if (value->pduBitmap & 0b1) {
    if (!(pull8(ppReadPackedMsg, &value->PTRSPortIndex, end) && pull8(ppReadPackedMsg, &value->PTRSTimeDensity, end)
          && pull8(ppReadPackedMsg, &value->PTRSFreqDensity, end) && pull8(ppReadPackedMsg, &value->PTRSReOffset, end)
          && pull8(ppReadPackedMsg, &value->nEpreRatioOfPDSCHToPTRS, end))) {
      return 0;
    }
  }

  // Preocding and Beamforming
  if(!unpack_nr_tx_beamforming_pdu(&value->precodingAndBeamforming, ppReadPackedMsg, end)) {
    return 0;
  }

  // Tx power info
  if (!(pull8(ppReadPackedMsg, &value->powerControlOffset, end) && pull8(ppReadPackedMsg, &value->powerControlOffsetSS, end))) {
    return 0;
  }

  // Check pduBitMap bit 1 to pull CBG parameters or not
  if (value->pduBitmap & 0b10) {
    if (!(pull8(ppReadPackedMsg, &value->isLastCbPresent, end) && pull8(ppReadPackedMsg, &value->isInlineTbCrc, end)
          && pull32(ppReadPackedMsg, &value->dlTbCrc, end))) {
      return 0;
    }
  }

  if (!pull8(ppReadPackedMsg, &value->maintenance_parms_v3.ldpcBaseGraph, end)
      || !pull32(ppReadPackedMsg, &value->maintenance_parms_v3.tbSizeLbrmBytes, end))
    return 0;

  return 1;
}

static uint8_t unpack_dl_tti_csi_rs_pdu_rel15_value(void *tlv, uint8_t **ppReadPackedMsg, uint8_t *end)
{
  nfapi_nr_dl_tti_csi_rs_pdu_rel15_t *value = (nfapi_nr_dl_tti_csi_rs_pdu_rel15_t *)tlv;
  if (!(pull16(ppReadPackedMsg, &value->bwp_size, end) && pull16(ppReadPackedMsg, &value->bwp_start, end)
        && pull8(ppReadPackedMsg, &value->subcarrier_spacing, end) && pull8(ppReadPackedMsg, &value->cyclic_prefix, end)
        && pull16(ppReadPackedMsg, &value->start_rb, end) && pull16(ppReadPackedMsg, &value->nr_of_rbs, end)
        && pull8(ppReadPackedMsg, &value->csi_type, end) && pull8(ppReadPackedMsg, &value->row, end)
        && pull16(ppReadPackedMsg, &value->freq_domain, end) && pull8(ppReadPackedMsg, &value->symb_l0, end)
        && pull8(ppReadPackedMsg, &value->symb_l1, end) && pull8(ppReadPackedMsg, &value->cdm_type, end)
        && pull8(ppReadPackedMsg, &value->freq_density, end) && pull16(ppReadPackedMsg, &value->scramb_id, end)
        && pull8(ppReadPackedMsg, &value->power_control_offset, end)
        && pull8(ppReadPackedMsg, &value->power_control_offset_ss, end))) {
    return 0;
  }

  // Preocding and Beamforming
  if(!unpack_nr_tx_beamforming_pdu(&value->precodingAndBeamforming, ppReadPackedMsg, end)) {
    return 0;
  }

  return 1;
}

static uint8_t unpack_dl_tti_ssb_pdu_rel15_value(void *tlv, uint8_t **ppReadPackedMsg, uint8_t *end)
{
  NFAPI_TRACE(NFAPI_TRACE_DEBUG, "Unpacking ssb. \n");
  uint8_t byte3, byte2, byte1, byte0;
  nfapi_nr_dl_tti_ssb_pdu_rel15_t *value = (nfapi_nr_dl_tti_ssb_pdu_rel15_t *)tlv;

  if (!(pull16(ppReadPackedMsg, &value->PhysCellId, end) && pull8(ppReadPackedMsg, &value->BetaPss, end)
        && pull8(ppReadPackedMsg, &value->SsbBlockIndex, end) && pull8(ppReadPackedMsg, &value->SsbSubcarrierOffset, end)
        && pull16(ppReadPackedMsg, &value->ssbOffsetPointA, end) && pull8(ppReadPackedMsg, &value->bchPayloadFlag, end)
        && pull8(ppReadPackedMsg, &byte3, end) && pull8(ppReadPackedMsg, &byte2, end) && pull8(ppReadPackedMsg, &byte1, end)
        && pull8(ppReadPackedMsg, &byte0, end))) { // this should be always 0, bchpayload is 24 bits
    return 0;
  }
  // rebuild the bchPayload
  value->bchPayload = byte3 << 16 | byte2 << 8 | byte1;

  // Preocding and Beamforming
  if(!unpack_nr_tx_beamforming_pdu(&value->precoding_and_beamforming, ppReadPackedMsg, end)) {
    return 0;
  }
  return 1;
}

static uint8_t unpack_dl_tti_request_body_value(uint8_t **ppReadPackedMsg, uint8_t *end, void *msg)
{
  nfapi_nr_dl_tti_request_pdu_t *value = (nfapi_nr_dl_tti_request_pdu_t *)msg;

  if (!(pull16(ppReadPackedMsg, &value->PDUType, end) && pull16(ppReadPackedMsg, (uint16_t *)&value->PDUSize, end)))
    return 0;

  // first match the pdu type, then call the respective function
  switch (value->PDUType) {
    case NFAPI_NR_DL_TTI_PDCCH_PDU_TYPE: {
      if (!(unpack_dl_tti_pdcch_pdu_rel15_value(&value->pdcch_pdu.pdcch_pdu_rel15, ppReadPackedMsg, end)))
        return 0;
    } break;

    case NFAPI_NR_DL_TTI_PDSCH_PDU_TYPE: {
      if (!(unpack_dl_tti_pdsch_pdu_rel15_value(&value->pdsch_pdu.pdsch_pdu_rel15, ppReadPackedMsg, end)))
        return 0;
    } break;

    case NFAPI_NR_DL_TTI_CSI_RS_PDU_TYPE: {
      if (!(unpack_dl_tti_csi_rs_pdu_rel15_value(&value->csi_rs_pdu.csi_rs_pdu_rel15, ppReadPackedMsg, end)))
        return 0;
    } break;

    case NFAPI_NR_DL_TTI_SSB_PDU_TYPE: {
      if (!(unpack_dl_tti_ssb_pdu_rel15_value(&value->ssb_pdu.ssb_pdu_rel15, ppReadPackedMsg, end)))
        return 0;
    } break;

    default: {
      NFAPI_TRACE(NFAPI_TRACE_ERROR, "FIXME : Invalid DL_TTI pdu type %d \n", value->PDUType);
    } break;
  }

  return 1;
}

uint8_t unpack_dl_tti_request(uint8_t **ppReadPackedMsg, uint8_t *end, void *msg, nfapi_p7_codec_config_t *config)
{
  nfapi_nr_dl_tti_request_t *pNfapiMsg = (nfapi_nr_dl_tti_request_t *)msg;
  pNfapiMsg->vendor_extension = NULL;
  if (!(pull16(ppReadPackedMsg, &pNfapiMsg->SFN, end) && pull16(ppReadPackedMsg, &pNfapiMsg->Slot, end)
        && pull8(ppReadPackedMsg, &pNfapiMsg->dl_tti_request_body.nPDUs, end)
        && pull8(ppReadPackedMsg, &pNfapiMsg->dl_tti_request_body.nGroup, end))) {
    return 0;
  }
  for (int i = 0; i < pNfapiMsg->dl_tti_request_body.nPDUs; i++) {
    if (!unpack_dl_tti_request_body_value(ppReadPackedMsg, end, &pNfapiMsg->dl_tti_request_body.dl_tti_pdu_list[i]))
      return 0;
  }

  for (int i = 0; i < pNfapiMsg->dl_tti_request_body.nGroup; i++) {
    if (!pull8(ppReadPackedMsg, &pNfapiMsg->dl_tti_request_body.nUe[i], end)) {
      return 0;
    }
    for (int j = 0; j < pNfapiMsg->dl_tti_request_body.nUe[i]; j++) {
      if (!pull8(ppReadPackedMsg, &pNfapiMsg->dl_tti_request_body.PduIdx[i][j], end)) {
        return 0;
      }
    }
  }

  return 1;
}
