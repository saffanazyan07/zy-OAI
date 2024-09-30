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
#include "nr_fapi_p7_utils.h"

static bool eq_dl_tti_beamforming(const nfapi_nr_tx_precoding_and_beamforming_t *a,
                                  const nfapi_nr_tx_precoding_and_beamforming_t *b)
{
  EQ(a->num_prgs, b->num_prgs);
  EQ(a->prg_size, b->prg_size);
  EQ(a->dig_bf_interfaces, b->dig_bf_interfaces);
  for (int prg = 0; prg < a->num_prgs; ++prg) {
    EQ(a->prgs_list[prg].pm_idx, b->prgs_list[prg].pm_idx);
    for (int dbf_if = 0; dbf_if < a->dig_bf_interfaces; ++dbf_if) {
      EQ(a->prgs_list[prg].dig_bf_interface_list[dbf_if].beam_idx, b->prgs_list[prg].dig_bf_interface_list[dbf_if].beam_idx);
    }
  }

  return true;
}

static bool eq_dl_tti_request_pdcch_pdu(const nfapi_nr_dl_tti_pdcch_pdu_rel15_t *a, const nfapi_nr_dl_tti_pdcch_pdu_rel15_t *b)
{
  EQ(a->BWPSize, b->BWPSize);
  EQ(a->BWPStart, b->BWPStart);
  EQ(a->SubcarrierSpacing, b->SubcarrierSpacing);
  EQ(a->CyclicPrefix, b->CyclicPrefix);
  EQ(a->StartSymbolIndex, b->StartSymbolIndex);
  EQ(a->DurationSymbols, b->DurationSymbols);
  for (int fdr_idx = 0; fdr_idx < 6; ++fdr_idx) {
    EQ(a->FreqDomainResource[fdr_idx], b->FreqDomainResource[fdr_idx]);
  }
  EQ(a->CceRegMappingType, b->CceRegMappingType);
  EQ(a->RegBundleSize, b->RegBundleSize);
  EQ(a->InterleaverSize, b->InterleaverSize);
  EQ(a->CoreSetType, b->CoreSetType);
  EQ(a->ShiftIndex, b->ShiftIndex);
  EQ(a->precoderGranularity, b->precoderGranularity);
  EQ(a->numDlDci, b->numDlDci);
  for (int dl_dci = 0; dl_dci < a->numDlDci; ++dl_dci) {
    const nfapi_nr_dl_dci_pdu_t *a_dci_pdu = &a->dci_pdu[dl_dci];
    const nfapi_nr_dl_dci_pdu_t *b_dci_pdu = &b->dci_pdu[dl_dci];
    EQ(a_dci_pdu->RNTI, b_dci_pdu->RNTI);
    EQ(a_dci_pdu->ScramblingId, b_dci_pdu->ScramblingId);
    EQ(a_dci_pdu->ScramblingRNTI, b_dci_pdu->ScramblingRNTI);
    EQ(a_dci_pdu->CceIndex, b_dci_pdu->CceIndex);
    EQ(a_dci_pdu->AggregationLevel, b_dci_pdu->AggregationLevel);
    EQ(eq_dl_tti_beamforming(&a_dci_pdu->precodingAndBeamforming, &b_dci_pdu->precodingAndBeamforming), true);
    EQ(a_dci_pdu->beta_PDCCH_1_0, b_dci_pdu->beta_PDCCH_1_0);
    EQ(a_dci_pdu->powerControlOffsetSS, b_dci_pdu->powerControlOffsetSS);
    EQ(a_dci_pdu->PayloadSizeBits, b_dci_pdu->PayloadSizeBits);
    for (int i = 0; i < 8; ++i) {
      // The parameter itself always has 8 positions, no need to calculate how many bytes the payload actually occupies
      EQ(a_dci_pdu->Payload[i], b_dci_pdu->Payload[i]);
    }
  }
  return true;
}

static bool eq_dl_tti_request_pdsch_pdu(const nfapi_nr_dl_tti_pdsch_pdu_rel15_t *a, const nfapi_nr_dl_tti_pdsch_pdu_rel15_t *b)
{
  EQ(a->pduBitmap, b->pduBitmap);
  EQ(a->rnti, b->rnti);
  EQ(a->pduIndex, b->pduIndex);
  EQ(a->BWPSize, b->BWPSize);
  EQ(a->BWPStart, b->BWPStart);
  EQ(a->SubcarrierSpacing, b->SubcarrierSpacing);
  EQ(a->CyclicPrefix, b->CyclicPrefix);
  EQ(a->NrOfCodewords, b->NrOfCodewords);
  for (int cw = 0; cw < a->NrOfCodewords; ++cw) {
    EQ(a->targetCodeRate[cw], b->targetCodeRate[cw]);
    EQ(a->qamModOrder[cw], b->qamModOrder[cw]);
    EQ(a->mcsIndex[cw], b->mcsIndex[cw]);
    EQ(a->mcsTable[cw], b->mcsTable[cw]);
    EQ(a->rvIndex[cw], b->rvIndex[cw]);
    EQ(a->TBSize[cw], b->TBSize[cw]);
  }
  EQ(a->dataScramblingId, b->dataScramblingId);
  EQ(a->nrOfLayers, b->nrOfLayers);
  EQ(a->transmissionScheme, b->transmissionScheme);
  EQ(a->refPoint, b->refPoint);
  EQ(a->dlDmrsSymbPos, b->dlDmrsSymbPos);
  EQ(a->dmrsConfigType, b->dmrsConfigType);
  EQ(a->dlDmrsScramblingId, b->dlDmrsScramblingId);
  EQ(a->SCID, b->SCID);
  EQ(a->numDmrsCdmGrpsNoData, b->numDmrsCdmGrpsNoData);
  EQ(a->dmrsPorts, b->dmrsPorts);
  EQ(a->resourceAlloc, b->resourceAlloc);
  for (int i = 0; i < 36; ++i) {
    EQ(a->rbBitmap[i], b->rbBitmap[i]);
  }
  EQ(a->rbStart, b->rbStart);
  EQ(a->rbSize, b->rbSize);
  EQ(a->VRBtoPRBMapping, b->VRBtoPRBMapping);
  EQ(a->StartSymbolIndex, b->StartSymbolIndex);
  EQ(a->NrOfSymbols, b->NrOfSymbols);
  EQ(a->PTRSPortIndex, b->PTRSPortIndex);
  EQ(a->PTRSTimeDensity, b->PTRSTimeDensity);
  EQ(a->PTRSFreqDensity, b->PTRSFreqDensity);
  EQ(a->PTRSReOffset, b->PTRSReOffset);
  EQ(a->nEpreRatioOfPDSCHToPTRS, b->nEpreRatioOfPDSCHToPTRS);
  EQ(eq_dl_tti_beamforming(&a->precodingAndBeamforming, &b->precodingAndBeamforming), true);
  EQ(a->powerControlOffset, b->powerControlOffset);
  EQ(a->powerControlOffsetSS, b->powerControlOffsetSS);
  EQ(a->isLastCbPresent, b->isLastCbPresent);
  EQ(a->isInlineTbCrc, b->isInlineTbCrc);
  EQ(a->dlTbCrc, b->dlTbCrc);
  EQ(a->maintenance_parms_v3.ldpcBaseGraph, b->maintenance_parms_v3.ldpcBaseGraph);
  EQ(a->maintenance_parms_v3.tbSizeLbrmBytes, b->maintenance_parms_v3.tbSizeLbrmBytes);
  return true;
}

static bool eq_dl_tti_request_csi_rs_pdu(const nfapi_nr_dl_tti_csi_rs_pdu_rel15_t *a, const nfapi_nr_dl_tti_csi_rs_pdu_rel15_t *b)
{
  EQ(a->bwp_size, b->bwp_size);
  EQ(a->bwp_start, b->bwp_start);
  EQ(a->subcarrier_spacing, b->subcarrier_spacing);
  EQ(a->cyclic_prefix, b->cyclic_prefix);
  EQ(a->start_rb, b->start_rb);
  EQ(a->nr_of_rbs, b->nr_of_rbs);
  EQ(a->csi_type, b->csi_type);
  EQ(a->row, b->row);
  EQ(a->freq_domain, b->freq_domain);
  EQ(a->symb_l0, b->symb_l0);
  EQ(a->symb_l1, b->symb_l1);
  EQ(a->cdm_type, b->cdm_type);
  EQ(a->freq_density, b->freq_density);
  EQ(a->scramb_id, b->scramb_id);
  EQ(a->power_control_offset, b->power_control_offset);
  EQ(a->power_control_offset_ss, b->power_control_offset_ss);
  EQ(eq_dl_tti_beamforming(&a->precodingAndBeamforming, &b->precodingAndBeamforming), true);
  return true;
}

static bool eq_dl_tti_request_ssb_pdu(const nfapi_nr_dl_tti_ssb_pdu_rel15_t *a, const nfapi_nr_dl_tti_ssb_pdu_rel15_t *b)
{
  EQ(a->PhysCellId, b->PhysCellId);
  EQ(a->BetaPss, b->BetaPss);
  EQ(a->SsbBlockIndex, b->SsbBlockIndex);
  EQ(a->SsbSubcarrierOffset, b->SsbSubcarrierOffset);
  EQ(a->ssbOffsetPointA, b->ssbOffsetPointA);
  EQ(a->bchPayloadFlag, b->bchPayloadFlag);
  EQ(a->bchPayload, b->bchPayload);
  EQ(a->ssbRsrp, b->ssbRsrp);
  EQ(eq_dl_tti_beamforming(&a->precoding_and_beamforming, &b->precoding_and_beamforming), true);
  return true;
}

bool eq_dl_tti_request(const nfapi_nr_dl_tti_request_t *a, const nfapi_nr_dl_tti_request_t *b)
{
  EQ(a->header.message_id, b->header.message_id);
  EQ(a->header.message_length, b->header.message_length);

  EQ(a->SFN, b->SFN);
  EQ(a->Slot, b->Slot);
  EQ(a->dl_tti_request_body.nPDUs, b->dl_tti_request_body.nPDUs);
  EQ(a->dl_tti_request_body.nGroup, b->dl_tti_request_body.nGroup);

  for (int PDU = 0; PDU < a->dl_tti_request_body.nPDUs; ++PDU) {
    // take the PDU at the start of loops
    const nfapi_nr_dl_tti_request_pdu_t *a_dl_pdu = &a->dl_tti_request_body.dl_tti_pdu_list[PDU];
    const nfapi_nr_dl_tti_request_pdu_t *b_dl_pdu = &b->dl_tti_request_body.dl_tti_pdu_list[PDU];

    EQ(a_dl_pdu->PDUType, b_dl_pdu->PDUType);
    EQ(a_dl_pdu->PDUSize, b_dl_pdu->PDUSize);

    switch (a_dl_pdu->PDUType) {
      case NFAPI_NR_DL_TTI_PDCCH_PDU_TYPE:
        EQ(eq_dl_tti_request_pdcch_pdu(&a_dl_pdu->pdcch_pdu.pdcch_pdu_rel15, &b_dl_pdu->pdcch_pdu.pdcch_pdu_rel15), true);
        break;
      case NFAPI_NR_DL_TTI_PDSCH_PDU_TYPE:
        EQ(eq_dl_tti_request_pdsch_pdu(&a_dl_pdu->pdsch_pdu.pdsch_pdu_rel15, &b_dl_pdu->pdsch_pdu.pdsch_pdu_rel15), true);
        break;
      case NFAPI_NR_DL_TTI_CSI_RS_PDU_TYPE:
        EQ(eq_dl_tti_request_csi_rs_pdu(&a_dl_pdu->csi_rs_pdu.csi_rs_pdu_rel15, &b_dl_pdu->csi_rs_pdu.csi_rs_pdu_rel15), true);
        break;
      case NFAPI_NR_DL_TTI_SSB_PDU_TYPE:
        EQ(eq_dl_tti_request_ssb_pdu(&a_dl_pdu->ssb_pdu.ssb_pdu_rel15, &b_dl_pdu->ssb_pdu.ssb_pdu_rel15), true);
        break;
      default:
        // PDU Type is not any known value
        return false;
    }
  }

  for (int nGroup = 0; nGroup < a->dl_tti_request_body.nGroup; ++nGroup) {
    EQ(a->dl_tti_request_body.nUe[nGroup], b->dl_tti_request_body.nUe[nGroup]);
    for (int UE = 0; UE < a->dl_tti_request_body.nUe[nGroup]; ++UE) {
      EQ(a->dl_tti_request_body.PduIdx[nGroup][UE], b->dl_tti_request_body.PduIdx[nGroup][UE]);
    }
  }

  return true;
}

void free_dl_tti_request(nfapi_nr_dl_tti_request_t *msg)
{
  if (msg->vendor_extension) {
    free(msg->vendor_extension);
  }
}

static void copy_dl_tti_beamforming(const nfapi_nr_tx_precoding_and_beamforming_t *src,
                                    nfapi_nr_tx_precoding_and_beamforming_t *dst)
{
  dst->num_prgs = src->num_prgs;
  dst->prg_size = src->prg_size;
  dst->dig_bf_interfaces = src->dig_bf_interfaces;
  for (int prg = 0; prg < dst->num_prgs; ++prg) {
    dst->prgs_list[prg].pm_idx = src->prgs_list[prg].pm_idx;
    for (int dbf_if = 0; dbf_if < dst->dig_bf_interfaces; ++dbf_if) {
      dst->prgs_list[prg].dig_bf_interface_list[dbf_if].beam_idx = src->prgs_list[prg].dig_bf_interface_list[dbf_if].beam_idx;
    }
  }
}

static void copy_dl_tti_request_pdcch_pdu(const nfapi_nr_dl_tti_pdcch_pdu_rel15_t *src, nfapi_nr_dl_tti_pdcch_pdu_rel15_t *dst)
{
  dst->BWPSize = src->BWPSize;
  dst->BWPStart = src->BWPStart;
  dst->SubcarrierSpacing = src->SubcarrierSpacing;
  dst->CyclicPrefix = src->CyclicPrefix;
  dst->StartSymbolIndex = src->StartSymbolIndex;
  dst->DurationSymbols = src->DurationSymbols;
  for (int fdr_idx = 0; fdr_idx < 6; ++fdr_idx) {
    dst->FreqDomainResource[fdr_idx] = src->FreqDomainResource[fdr_idx];
  }
  dst->CceRegMappingType = src->CceRegMappingType;
  dst->RegBundleSize = src->RegBundleSize;
  dst->InterleaverSize = src->InterleaverSize;
  dst->CoreSetType = src->CoreSetType;
  dst->ShiftIndex = src->ShiftIndex;
  dst->precoderGranularity = src->precoderGranularity;
  dst->numDlDci = src->numDlDci;
  for (int dl_dci = 0; dl_dci < dst->numDlDci; ++dl_dci) {
    nfapi_nr_dl_dci_pdu_t *dst_dci_pdu = &dst->dci_pdu[dl_dci];
    const nfapi_nr_dl_dci_pdu_t *src_dci_pdu = &src->dci_pdu[dl_dci];
    dst_dci_pdu->RNTI = src_dci_pdu->RNTI;
    dst_dci_pdu->ScramblingId = src_dci_pdu->ScramblingId;
    dst_dci_pdu->ScramblingRNTI = src_dci_pdu->ScramblingRNTI;
    dst_dci_pdu->CceIndex = src_dci_pdu->CceIndex;
    dst_dci_pdu->AggregationLevel = src_dci_pdu->AggregationLevel;
    copy_dl_tti_beamforming(&src_dci_pdu->precodingAndBeamforming, &dst_dci_pdu->precodingAndBeamforming);
    dst_dci_pdu->beta_PDCCH_1_0 = src_dci_pdu->beta_PDCCH_1_0;
    dst_dci_pdu->powerControlOffsetSS = src_dci_pdu->powerControlOffsetSS;
    dst_dci_pdu->PayloadSizeBits = src_dci_pdu->PayloadSizeBits;
    for (int i = 0; i < 8; ++i) {
      dst_dci_pdu->Payload[i] = src_dci_pdu->Payload[i];
    }
  }
}

static void copy_dl_tti_request_pdsch_pdu(const nfapi_nr_dl_tti_pdsch_pdu_rel15_t *src, nfapi_nr_dl_tti_pdsch_pdu_rel15_t *dst)
{
  dst->pduBitmap = src->pduBitmap;
  dst->rnti = src->rnti;
  dst->pduIndex = src->pduIndex;
  dst->BWPSize = src->BWPSize;
  dst->BWPStart = src->BWPStart;
  dst->SubcarrierSpacing = src->SubcarrierSpacing;
  dst->CyclicPrefix = src->CyclicPrefix;
  dst->NrOfCodewords = src->NrOfCodewords;
  for (int cw = 0; cw < dst->NrOfCodewords; ++cw) {
    dst->targetCodeRate[cw] = src->targetCodeRate[cw];
    dst->qamModOrder[cw] = src->qamModOrder[cw];
    dst->mcsIndex[cw] = src->mcsIndex[cw];
    dst->mcsTable[cw] = src->mcsTable[cw];
    dst->rvIndex[cw] = src->rvIndex[cw];
    dst->TBSize[cw] = src->TBSize[cw];
  }
  dst->dataScramblingId = src->dataScramblingId;
  dst->nrOfLayers = src->nrOfLayers;
  dst->transmissionScheme = src->transmissionScheme;
  dst->refPoint = src->refPoint;
  dst->dlDmrsSymbPos = src->dlDmrsSymbPos;
  dst->dmrsConfigType = src->dmrsConfigType;
  dst->dlDmrsScramblingId = src->dlDmrsScramblingId;
  dst->SCID = src->SCID;
  dst->numDmrsCdmGrpsNoData = src->numDmrsCdmGrpsNoData;
  dst->dmrsPorts = src->dmrsPorts;
  dst->resourceAlloc = src->resourceAlloc;
  for (int i = 0; i < 36; ++i) {
    dst->rbBitmap[i] = src->rbBitmap[i];
  }
  dst->rbStart = src->rbStart;
  dst->rbSize = src->rbSize;
  dst->VRBtoPRBMapping = src->VRBtoPRBMapping;
  dst->StartSymbolIndex = src->StartSymbolIndex;
  dst->NrOfSymbols = src->NrOfSymbols;
  dst->PTRSPortIndex = src->PTRSPortIndex;
  dst->PTRSTimeDensity = src->PTRSTimeDensity;
  dst->PTRSFreqDensity = src->PTRSFreqDensity;
  dst->PTRSReOffset = src->PTRSReOffset;
  dst->nEpreRatioOfPDSCHToPTRS = src->nEpreRatioOfPDSCHToPTRS;
  copy_dl_tti_beamforming(&src->precodingAndBeamforming, &dst->precodingAndBeamforming);
  dst->powerControlOffset = src->powerControlOffset;
  dst->powerControlOffsetSS = src->powerControlOffsetSS;
  dst->isLastCbPresent = src->isLastCbPresent;
  dst->isInlineTbCrc = src->isInlineTbCrc;
  dst->dlTbCrc = src->dlTbCrc;
  dst->maintenance_parms_v3.ldpcBaseGraph = src->maintenance_parms_v3.ldpcBaseGraph;
  dst->maintenance_parms_v3.tbSizeLbrmBytes = src->maintenance_parms_v3.tbSizeLbrmBytes;
}

static void copy_dl_tti_request_csi_rs_pdu(const nfapi_nr_dl_tti_csi_rs_pdu_rel15_t *src, nfapi_nr_dl_tti_csi_rs_pdu_rel15_t *dst)
{
  dst->bwp_size = src->bwp_size;
  dst->bwp_start = src->bwp_start;
  dst->subcarrier_spacing = src->subcarrier_spacing;
  dst->cyclic_prefix = src->cyclic_prefix;
  dst->start_rb = src->start_rb;
  dst->nr_of_rbs = src->nr_of_rbs;
  dst->csi_type = src->csi_type;
  dst->row = src->row;
  dst->freq_domain = src->freq_domain;
  dst->symb_l0 = src->symb_l0;
  dst->symb_l1 = src->symb_l1;
  dst->cdm_type = src->cdm_type;
  dst->freq_density = src->freq_density;
  dst->scramb_id = src->scramb_id;
  dst->power_control_offset = src->power_control_offset;
  dst->power_control_offset_ss = src->power_control_offset_ss;
  copy_dl_tti_beamforming(&src->precodingAndBeamforming, &dst->precodingAndBeamforming);
}

static void copy_dl_tti_request_ssb_pdu(const nfapi_nr_dl_tti_ssb_pdu_rel15_t *src, nfapi_nr_dl_tti_ssb_pdu_rel15_t *dst)
{
  dst->PhysCellId = src->PhysCellId;
  dst->BetaPss = src->BetaPss;
  dst->SsbBlockIndex = src->SsbBlockIndex;
  dst->SsbSubcarrierOffset = src->SsbSubcarrierOffset;
  dst->ssbOffsetPointA = src->ssbOffsetPointA;
  dst->bchPayloadFlag = src->bchPayloadFlag;
  dst->bchPayload = src->bchPayload;
  dst->ssbRsrp = src->ssbRsrp;
  copy_dl_tti_beamforming(&src->precoding_and_beamforming, &dst->precoding_and_beamforming);
}

static void copy_dl_tti_request_pdu(const nfapi_nr_dl_tti_request_pdu_t *src, nfapi_nr_dl_tti_request_pdu_t *dst)
{
  dst->PDUType = src->PDUType;
  dst->PDUSize = src->PDUSize;

  switch (dst->PDUType) {
    case NFAPI_NR_DL_TTI_PDCCH_PDU_TYPE:
      copy_dl_tti_request_pdcch_pdu(&src->pdcch_pdu.pdcch_pdu_rel15, &dst->pdcch_pdu.pdcch_pdu_rel15);
      break;
    case NFAPI_NR_DL_TTI_PDSCH_PDU_TYPE:
      copy_dl_tti_request_pdsch_pdu(&src->pdsch_pdu.pdsch_pdu_rel15, &dst->pdsch_pdu.pdsch_pdu_rel15);
      break;
    case NFAPI_NR_DL_TTI_CSI_RS_PDU_TYPE:
      copy_dl_tti_request_csi_rs_pdu(&src->csi_rs_pdu.csi_rs_pdu_rel15, &dst->csi_rs_pdu.csi_rs_pdu_rel15);
      break;
    case NFAPI_NR_DL_TTI_SSB_PDU_TYPE:
      copy_dl_tti_request_ssb_pdu(&src->ssb_pdu.ssb_pdu_rel15, &dst->ssb_pdu.ssb_pdu_rel15);
      break;
    default:
      // PDU Type is not any known value
      AssertFatal(1 == 0, "PDU Type value unknown, allowed values range from 0 to 3\n");
  }
}

void copy_dl_tti_request(const nfapi_nr_dl_tti_request_t *src, nfapi_nr_dl_tti_request_t *dst)
{
  dst->header.message_id = src->header.message_id;
  dst->header.message_length = src->header.message_length;
  if (src->vendor_extension) {
    dst->vendor_extension = calloc(1, sizeof(nfapi_vendor_extension_tlv_t));
    dst->vendor_extension->tag = src->vendor_extension->tag;
    dst->vendor_extension->length = src->vendor_extension->length;
    copy_vendor_extension_value(&dst->vendor_extension, &src->vendor_extension);
  }

  dst->SFN = src->SFN;
  dst->Slot = src->Slot;
  dst->dl_tti_request_body.nPDUs = src->dl_tti_request_body.nPDUs;
  dst->dl_tti_request_body.nGroup = src->dl_tti_request_body.nGroup;
  for (int pdu = 0; pdu < dst->dl_tti_request_body.nPDUs; ++pdu) {
    copy_dl_tti_request_pdu(&src->dl_tti_request_body.dl_tti_pdu_list[pdu], &dst->dl_tti_request_body.dl_tti_pdu_list[pdu]);
  }
  if (dst->dl_tti_request_body.nGroup > 0) {
    for (int nGroup = 0; nGroup < dst->dl_tti_request_body.nGroup; ++nGroup) {
      dst->dl_tti_request_body.nUe[nGroup] = src->dl_tti_request_body.nUe[nGroup];
      for (int UE = 0; UE < dst->dl_tti_request_body.nUe[nGroup]; ++UE) {
        dst->dl_tti_request_body.PduIdx[nGroup][UE] = src->dl_tti_request_body.PduIdx[nGroup][UE];
      }
    }
  }
}
