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

static bool eq_ul_tti_beamforming(const nfapi_nr_ul_beamforming_t *a, const nfapi_nr_ul_beamforming_t *b)
{
  EQ(a->trp_scheme, b->trp_scheme);
  EQ(a->num_prgs, b->num_prgs);
  EQ(a->prg_size, b->prg_size);
  EQ(a->dig_bf_interface, b->dig_bf_interface);
  for (int prg = 0; prg < a->num_prgs; ++prg) {
    for (int dbf_if = 0; dbf_if < a->dig_bf_interface; ++dbf_if) {
      EQ(a->prgs_list[prg].dig_bf_interface_list[dbf_if].beam_idx, b->prgs_list[prg].dig_bf_interface_list[dbf_if].beam_idx);
    }
  }

  return true;
}

static bool eq_ul_tti_request_prach_pdu(const nfapi_nr_prach_pdu_t *a, const nfapi_nr_prach_pdu_t *b)
{
  EQ(a->phys_cell_id, b->phys_cell_id);
  EQ(a->num_prach_ocas, b->num_prach_ocas);
  EQ(a->prach_format, b->prach_format);
  EQ(a->num_ra, b->num_ra);
  EQ(a->prach_start_symbol, b->prach_start_symbol);
  EQ(a->num_cs, b->num_cs);
  EQ(eq_ul_tti_beamforming(&a->beamforming, &b->beamforming), true);
  return true;
}

static bool eq_ul_tti_request_pusch_pdu(const nfapi_nr_pusch_pdu_t *a, const nfapi_nr_pusch_pdu_t *b)
{
  EQ(a->pdu_bit_map, b->pdu_bit_map);
  EQ(a->rnti, b->rnti);
  EQ(a->handle, b->handle);
  EQ(a->bwp_size, b->bwp_size);
  EQ(a->bwp_start, b->bwp_start);
  EQ(a->subcarrier_spacing, b->subcarrier_spacing);
  EQ(a->cyclic_prefix, b->cyclic_prefix);
  EQ(a->target_code_rate, b->target_code_rate);
  EQ(a->qam_mod_order, b->qam_mod_order);
  EQ(a->mcs_index, b->mcs_index);
  EQ(a->mcs_table, b->mcs_table);
  EQ(a->transform_precoding, b->transform_precoding);
  EQ(a->data_scrambling_id, b->data_scrambling_id);
  EQ(a->nrOfLayers, b->nrOfLayers);
  EQ(a->ul_dmrs_symb_pos, b->ul_dmrs_symb_pos);
  EQ(a->dmrs_config_type, b->dmrs_config_type);
  EQ(a->ul_dmrs_scrambling_id, b->ul_dmrs_scrambling_id);
  EQ(a->pusch_identity, b->pusch_identity);
  EQ(a->scid, b->scid);
  EQ(a->num_dmrs_cdm_grps_no_data, b->num_dmrs_cdm_grps_no_data);
  EQ(a->dmrs_ports, b->dmrs_ports);
  EQ(a->resource_alloc, b->resource_alloc);
  for (int i = 0; i < 36; ++i) {
    EQ(a->rb_bitmap[i], b->rb_bitmap[i]);
  }
  EQ(a->rb_start, b->rb_start);
  EQ(a->rb_size, b->rb_size);
  EQ(a->vrb_to_prb_mapping, b->vrb_to_prb_mapping);
  EQ(a->frequency_hopping, b->frequency_hopping);
  EQ(a->tx_direct_current_location, b->tx_direct_current_location);
  EQ(a->uplink_frequency_shift_7p5khz, b->uplink_frequency_shift_7p5khz);
  EQ(a->start_symbol_index, b->start_symbol_index);
  EQ(a->nr_of_symbols, b->nr_of_symbols);

  if (a->pdu_bit_map & PUSCH_PDU_BITMAP_PUSCH_DATA) {
    const nfapi_nr_pusch_data_t *a_pusch_data = &a->pusch_data;
    const nfapi_nr_pusch_data_t *b_pusch_data = &b->pusch_data;
    EQ(a_pusch_data->rv_index, b_pusch_data->rv_index);
    EQ(a_pusch_data->harq_process_id, b_pusch_data->harq_process_id);
    EQ(a_pusch_data->new_data_indicator, b_pusch_data->new_data_indicator);
    EQ(a_pusch_data->tb_size, b_pusch_data->tb_size);
    EQ(a_pusch_data->num_cb, b_pusch_data->num_cb);
    for (int i = 0; i < (a_pusch_data->num_cb + 7) / 8; ++i) {
      EQ(a_pusch_data->cb_present_and_position[i], b_pusch_data->cb_present_and_position[i]);
    }
  }
  if (a->pdu_bit_map & PUSCH_PDU_BITMAP_PUSCH_UCI) {
    const nfapi_nr_pusch_uci_t *a_pusch_uci = &a->pusch_uci;
    const nfapi_nr_pusch_uci_t *b_pusch_uci = &b->pusch_uci;
    EQ(a_pusch_uci->harq_ack_bit_length, b_pusch_uci->harq_ack_bit_length);
    EQ(a_pusch_uci->csi_part1_bit_length, b_pusch_uci->csi_part1_bit_length);
    EQ(a_pusch_uci->csi_part2_bit_length, b_pusch_uci->csi_part2_bit_length);
    EQ(a_pusch_uci->alpha_scaling, b_pusch_uci->alpha_scaling);
    EQ(a_pusch_uci->beta_offset_harq_ack, b_pusch_uci->beta_offset_harq_ack);
    EQ(a_pusch_uci->beta_offset_csi1, b_pusch_uci->beta_offset_csi1);
    EQ(a_pusch_uci->beta_offset_csi2, b_pusch_uci->beta_offset_csi2);
  }
  if (a->pdu_bit_map & PUSCH_PDU_BITMAP_PUSCH_PTRS) {
    const nfapi_nr_pusch_ptrs_t *a_pusch_ptrs = &a->pusch_ptrs;
    const nfapi_nr_pusch_ptrs_t *b_pusch_ptrs = &b->pusch_ptrs;

    EQ(a_pusch_ptrs->num_ptrs_ports, b_pusch_ptrs->num_ptrs_ports);
    for (int i = 0; i < a_pusch_ptrs->num_ptrs_ports; ++i) {
      const nfapi_nr_ptrs_ports_t *a_ptrs_port = &a_pusch_ptrs->ptrs_ports_list[i];
      const nfapi_nr_ptrs_ports_t *b_ptrs_port = &b_pusch_ptrs->ptrs_ports_list[i];

      EQ(a_ptrs_port->ptrs_port_index, b_ptrs_port->ptrs_port_index);
      EQ(a_ptrs_port->ptrs_dmrs_port, b_ptrs_port->ptrs_dmrs_port);
      EQ(a_ptrs_port->ptrs_re_offset, b_ptrs_port->ptrs_re_offset);
    }

    EQ(a_pusch_ptrs->ptrs_time_density, b_pusch_ptrs->ptrs_time_density);
    EQ(a_pusch_ptrs->ptrs_freq_density, b_pusch_ptrs->ptrs_freq_density);
    EQ(a_pusch_ptrs->ul_ptrs_power, b_pusch_ptrs->ul_ptrs_power);
  }
  if (a->pdu_bit_map & PUSCH_PDU_BITMAP_DFTS_OFDM) {
    const nfapi_nr_dfts_ofdm_t *a_dfts_ofdm = &a->dfts_ofdm;
    const nfapi_nr_dfts_ofdm_t *b_dfts_ofdm = &b->dfts_ofdm;

    EQ(a_dfts_ofdm->low_papr_group_number, b_dfts_ofdm->low_papr_group_number);
    EQ(a_dfts_ofdm->low_papr_sequence_number, b_dfts_ofdm->low_papr_sequence_number);
    EQ(a_dfts_ofdm->ul_ptrs_sample_density, b_dfts_ofdm->ul_ptrs_sample_density);
    EQ(a_dfts_ofdm->ul_ptrs_time_density_transform_precoding, b_dfts_ofdm->ul_ptrs_time_density_transform_precoding);
  }
  EQ(eq_ul_tti_beamforming(&a->beamforming, &b->beamforming), true);
  EQ(a->maintenance_parms_v3.ldpcBaseGraph, b->maintenance_parms_v3.ldpcBaseGraph);
  EQ(a->maintenance_parms_v3.tbSizeLbrmBytes, b->maintenance_parms_v3.tbSizeLbrmBytes);
  return true;
}

static bool eq_ul_tti_request_pucch_pdu(const nfapi_nr_pucch_pdu_t *a, const nfapi_nr_pucch_pdu_t *b)
{
  EQ(a->rnti, b->rnti);
  EQ(a->handle, b->handle);
  EQ(a->bwp_size, b->bwp_size);
  EQ(a->bwp_start, b->bwp_start);
  EQ(a->subcarrier_spacing, b->subcarrier_spacing);
  EQ(a->cyclic_prefix, b->cyclic_prefix);
  EQ(a->format_type, b->format_type);
  EQ(a->multi_slot_tx_indicator, b->multi_slot_tx_indicator);
  EQ(a->pi_2bpsk, b->pi_2bpsk);
  EQ(a->prb_start, b->prb_start);
  EQ(a->prb_size, b->prb_size);
  EQ(a->start_symbol_index, b->start_symbol_index);
  EQ(a->nr_of_symbols, b->nr_of_symbols);
  EQ(a->freq_hop_flag, b->freq_hop_flag);
  EQ(a->second_hop_prb, b->second_hop_prb);
  EQ(a->group_hop_flag, b->group_hop_flag);
  EQ(a->sequence_hop_flag, b->sequence_hop_flag);
  EQ(a->hopping_id, b->hopping_id);
  EQ(a->initial_cyclic_shift, b->initial_cyclic_shift);
  EQ(a->data_scrambling_id, b->data_scrambling_id);
  EQ(a->time_domain_occ_idx, b->time_domain_occ_idx);
  EQ(a->pre_dft_occ_idx, b->pre_dft_occ_idx);
  EQ(a->pre_dft_occ_len, b->pre_dft_occ_len);
  EQ(a->add_dmrs_flag, b->add_dmrs_flag);
  EQ(a->dmrs_scrambling_id, b->dmrs_scrambling_id);
  EQ(a->dmrs_cyclic_shift, b->dmrs_cyclic_shift);
  EQ(a->sr_flag, b->sr_flag);
  EQ(a->bit_len_harq, b->bit_len_harq);
  EQ(a->bit_len_csi_part1, b->bit_len_csi_part1);
  EQ(a->bit_len_csi_part2, b->bit_len_csi_part2);
  EQ(eq_ul_tti_beamforming(&a->beamforming, &b->beamforming), true);
  return true;
}

static bool eq_ul_tti_request_srs_parameters(const nfapi_v4_srs_parameters_t *a,
                                             const uint8_t num_symbols,
                                             const nfapi_v4_srs_parameters_t *b)
{
  EQ(a->srs_bandwidth_size, b->srs_bandwidth_size);
  for (int symbol_idx = 0; symbol_idx < num_symbols; ++symbol_idx) {
    const nfapi_v4_srs_parameters_symbols_t *a_symbol = &a->symbol_list[symbol_idx];
    const nfapi_v4_srs_parameters_symbols_t *b_symbol = &b->symbol_list[symbol_idx];
    EQ(a_symbol->srs_bandwidth_start, b_symbol->srs_bandwidth_start);
    EQ(a_symbol->sequence_group, b_symbol->sequence_group);
    EQ(a_symbol->sequence_number, b_symbol->sequence_number);
  }

#ifdef ENABLE_AERIAL
  // For Aerial, we always process the 4 reported symbols, not only the ones indicated by num_symbols
  for (int symbol_idx = num_symbols; symbol_idx < 4; ++symbol_idx) {
    const nfapi_v4_srs_parameters_symbols_t *a_symbol = &a->symbol_list[symbol_idx];
    const nfapi_v4_srs_parameters_symbols_t *b_symbol = &b->symbol_list[symbol_idx];
    EQ(a_symbol->srs_bandwidth_start, b_symbol->srs_bandwidth_start);
    EQ(a_symbol->sequence_group, b_symbol->sequence_group);
    EQ(a_symbol->sequence_number, b_symbol->sequence_number);
  }
#endif // ENABLE_AERIAL

  EQ(a->usage, b->usage);
  const uint8_t nUsage = __builtin_popcount(a->usage);
  for (int idx = 0; idx < nUsage; ++idx) {
    EQ(a->report_type[idx], b->report_type[idx]);
  }
  EQ(a->singular_Value_representation, b->singular_Value_representation);
  EQ(a->iq_representation, b->iq_representation);
  EQ(a->prg_size, b->prg_size);
  EQ(a->num_total_ue_antennas, b->num_total_ue_antennas);
  EQ(a->ue_antennas_in_this_srs_resource_set, b->ue_antennas_in_this_srs_resource_set);
  EQ(a->sampled_ue_antennas, b->sampled_ue_antennas);
  EQ(a->report_scope, b->report_scope);
  EQ(a->num_ul_spatial_streams_ports, b->num_ul_spatial_streams_ports);
  for (int idx = 0; idx < a->num_ul_spatial_streams_ports; ++idx) {
    EQ(a->Ul_spatial_stream_ports[idx], b->Ul_spatial_stream_ports[idx]);
  }
  return true;
}

static bool eq_ul_tti_request_srs_pdu(const nfapi_nr_srs_pdu_t *a, const nfapi_nr_srs_pdu_t *b)
{
  EQ(a->rnti, b->rnti);
  EQ(a->handle, b->handle);
  EQ(a->bwp_size, b->bwp_size);
  EQ(a->bwp_start, b->bwp_start);
  EQ(a->subcarrier_spacing, b->subcarrier_spacing);
  EQ(a->cyclic_prefix, b->cyclic_prefix);
  EQ(a->num_ant_ports, b->num_ant_ports);
  EQ(a->num_symbols, b->num_symbols);
  EQ(a->num_repetitions, b->num_repetitions);
  EQ(a->time_start_position, b->time_start_position);
  EQ(a->config_index, b->config_index);
  EQ(a->sequence_id, b->sequence_id);
  EQ(a->bandwidth_index, b->bandwidth_index);
  EQ(a->comb_size, b->comb_size);
  EQ(a->comb_offset, b->comb_offset);
  EQ(a->cyclic_shift, b->cyclic_shift);
  EQ(a->frequency_position, b->frequency_position);
  EQ(a->frequency_shift, b->frequency_shift);
  EQ(a->frequency_hopping, b->frequency_hopping);
  EQ(a->group_or_sequence_hopping, b->group_or_sequence_hopping);
  EQ(a->resource_type, b->resource_type);
  EQ(a->t_srs, b->t_srs);
  EQ(a->t_offset, b->t_offset);
  EQ(eq_ul_tti_beamforming(&a->beamforming, &b->beamforming), true);
  EQ(eq_ul_tti_request_srs_parameters(&a->srs_parameters_v4, 1 << a->num_symbols, &b->srs_parameters_v4), true);
  return true;
}

bool eq_ul_tti_request(const nfapi_nr_ul_tti_request_t *a, const nfapi_nr_ul_tti_request_t *b)
{
  EQ(a->header.message_id, b->header.message_id);
  EQ(a->header.message_length, b->header.message_length);

  EQ(a->SFN, b->SFN);
  EQ(a->Slot, b->Slot);
  EQ(a->n_pdus, b->n_pdus);
  EQ(a->rach_present, b->rach_present);
  EQ(a->n_ulsch, b->n_ulsch);
  EQ(a->n_ulcch, b->n_ulcch);
  EQ(a->n_group, b->n_group);

  for (int PDU = 0; PDU < a->n_pdus; ++PDU) {
    // take the PDU at the start of loops
    const nfapi_nr_ul_tti_request_number_of_pdus_t *a_pdu = &a->pdus_list[PDU];
    const nfapi_nr_ul_tti_request_number_of_pdus_t *b_pdu = &b->pdus_list[PDU];

    EQ(a_pdu->pdu_type, b_pdu->pdu_type);
    EQ(a_pdu->pdu_size, b_pdu->pdu_size);

    switch (a_pdu->pdu_type) {
      case NFAPI_NR_UL_CONFIG_PRACH_PDU_TYPE:
        EQ(eq_ul_tti_request_prach_pdu(&a_pdu->prach_pdu, &b_pdu->prach_pdu), true);
        break;
      case NFAPI_NR_UL_CONFIG_PUSCH_PDU_TYPE:
        EQ(eq_ul_tti_request_pusch_pdu(&a_pdu->pusch_pdu, &b_pdu->pusch_pdu), true);
        break;
      case NFAPI_NR_UL_CONFIG_PUCCH_PDU_TYPE:
        EQ(eq_ul_tti_request_pucch_pdu(&a_pdu->pucch_pdu, &b_pdu->pucch_pdu), true);
        break;
      case NFAPI_NR_UL_CONFIG_SRS_PDU_TYPE:
        EQ(eq_ul_tti_request_srs_pdu(&a_pdu->srs_pdu, &b_pdu->srs_pdu), true);
        break;
      default:
        // PDU Type is not any known value
        return false;
    }
  }

  for (int nGroup = 0; nGroup < a->n_group; ++nGroup) {
    EQ(a->groups_list[nGroup].n_ue, b->groups_list[nGroup].n_ue);
    for (int UE = 0; UE < a->groups_list[nGroup].n_ue; ++UE) {
      EQ(a->groups_list[nGroup].ue_list[UE].pdu_idx, b->groups_list[nGroup].ue_list[UE].pdu_idx);
    }
  }
  return true;
}

bool eq_slot_indication(const nfapi_nr_slot_indication_scf_t *a, const nfapi_nr_slot_indication_scf_t *b)
{
  EQ(a->header.message_id, b->header.message_id);
  EQ(a->header.message_length, b->header.message_length);

  EQ(a->sfn, b->sfn);
  EQ(a->slot, b->slot);
  return true;
}

bool eq_ul_dci_request_PDU(const nfapi_nr_ul_dci_request_pdus_t *a, const nfapi_nr_ul_dci_request_pdus_t *b)
{
  EQ(a->PDUType, b->PDUType);
  EQ(a->PDUSize, b->PDUSize);
  // Is the same PDU as the DL_TTI.request PDCCH PDU, reuse comparison function
  EQ(eq_dl_tti_request_pdcch_pdu(&a->pdcch_pdu.pdcch_pdu_rel15, &b->pdcch_pdu.pdcch_pdu_rel15), true);
  return true;
}

bool eq_ul_dci_request(const nfapi_nr_ul_dci_request_t *a, const nfapi_nr_ul_dci_request_t *b)
{
  EQ(a->header.message_id, b->header.message_id);
  EQ(a->header.message_length, b->header.message_length);

  EQ(a->SFN, b->SFN);
  EQ(a->Slot, b->Slot);
  EQ(a->numPdus, b->numPdus);
  for (int pdu_idx = 0; pdu_idx < a->numPdus; ++pdu_idx) {
    EQ(eq_ul_dci_request_PDU(&a->ul_dci_pdu_list[pdu_idx], &b->ul_dci_pdu_list[pdu_idx]), true);
  }
  return true;
}

void free_dl_tti_request(nfapi_nr_dl_tti_request_t *msg)
{
  if (msg->vendor_extension) {
    free(msg->vendor_extension);
  }
}

void free_ul_tti_request(nfapi_nr_ul_tti_request_t *msg)
{
  for (int idx_pdu = 0; idx_pdu < msg->n_pdus; ++idx_pdu) {
    nfapi_nr_ul_tti_request_number_of_pdus_t *pdu = &msg->pdus_list[idx_pdu];
    switch (pdu->pdu_type) {
      case NFAPI_NR_UL_CONFIG_PUSCH_PDU_TYPE:
        free(pdu->pusch_pdu.pusch_ptrs.ptrs_ports_list);
        break;
      default:
        break;
    }
  }
}

void free_slot_indication(nfapi_nr_slot_indication_scf_t *msg)
{
  // Nothing to free
}

void free_ul_dci_request(nfapi_nr_ul_dci_request_t *msg)
{
  // Nothing to free
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

static void copy_ul_tti_beamforming(const nfapi_nr_ul_beamforming_t *src, nfapi_nr_ul_beamforming_t *dst)
{
  dst->trp_scheme = src->trp_scheme;
  dst->num_prgs = src->num_prgs;
  dst->prg_size = src->prg_size;
  dst->dig_bf_interface = src->dig_bf_interface;

  for (int prg = 0; prg < dst->num_prgs; ++prg) {
    if (dst->dig_bf_interface > 0) {
      for (int dbf_if = 0; dbf_if < dst->dig_bf_interface; ++dbf_if) {
        dst->prgs_list[prg].dig_bf_interface_list[dbf_if].beam_idx = src->prgs_list[prg].dig_bf_interface_list[dbf_if].beam_idx;
      }
    }
  }
}

static void copy_ul_tti_request_prach_pdu(const nfapi_nr_prach_pdu_t *src, nfapi_nr_prach_pdu_t *dst)
{
  dst->phys_cell_id = src->phys_cell_id;
  dst->num_prach_ocas = src->num_prach_ocas;
  dst->prach_format = src->prach_format;
  dst->num_ra = src->num_ra;
  dst->prach_start_symbol = src->prach_start_symbol;
  dst->num_cs = src->num_cs;
  copy_ul_tti_beamforming(&src->beamforming, &dst->beamforming);
}

static void copy_ul_tti_request_pusch_pdu(const nfapi_nr_pusch_pdu_t *src, nfapi_nr_pusch_pdu_t *dst)
{
  dst->pdu_bit_map = src->pdu_bit_map;
  dst->rnti = src->rnti;
  dst->handle = src->handle;
  dst->bwp_size = src->bwp_size;
  dst->bwp_start = src->bwp_start;
  dst->subcarrier_spacing = src->subcarrier_spacing;
  dst->cyclic_prefix = src->cyclic_prefix;
  dst->target_code_rate = src->target_code_rate;
  dst->qam_mod_order = src->qam_mod_order;
  dst->mcs_index = src->mcs_index;
  dst->mcs_table = src->mcs_table;
  dst->transform_precoding = src->transform_precoding;
  dst->data_scrambling_id = src->data_scrambling_id;
  dst->nrOfLayers = src->nrOfLayers;
  dst->ul_dmrs_symb_pos = src->ul_dmrs_symb_pos;
  dst->dmrs_config_type = src->dmrs_config_type;
  dst->ul_dmrs_scrambling_id = src->ul_dmrs_scrambling_id;
  dst->pusch_identity = src->pusch_identity;
  dst->scid = src->scid;
  dst->num_dmrs_cdm_grps_no_data = src->num_dmrs_cdm_grps_no_data;
  dst->dmrs_ports = src->dmrs_ports;
  dst->resource_alloc = src->resource_alloc;
  for (int i = 0; i < 36; ++i) {
    dst->rb_bitmap[i] = src->rb_bitmap[i];
  }
  dst->rb_start = src->rb_start;
  dst->rb_size = src->rb_size;
  dst->vrb_to_prb_mapping = src->vrb_to_prb_mapping;
  dst->frequency_hopping = src->frequency_hopping;
  dst->tx_direct_current_location = src->tx_direct_current_location;
  dst->uplink_frequency_shift_7p5khz = src->uplink_frequency_shift_7p5khz;
  dst->start_symbol_index = src->start_symbol_index;
  dst->nr_of_symbols = src->nr_of_symbols;

  if (dst->pdu_bit_map & PUSCH_PDU_BITMAP_PUSCH_DATA) {
    const nfapi_nr_pusch_data_t *src_pusch_data = &src->pusch_data;
    nfapi_nr_pusch_data_t *dst_pusch_data = &dst->pusch_data;
    dst_pusch_data->rv_index = src_pusch_data->rv_index;
    dst_pusch_data->harq_process_id = src_pusch_data->harq_process_id;
    dst_pusch_data->new_data_indicator = src_pusch_data->new_data_indicator;
    dst_pusch_data->tb_size = src_pusch_data->tb_size;
    dst_pusch_data->num_cb = src_pusch_data->num_cb;
    for (int i = 0; i < (src_pusch_data->num_cb + 7) / 8; ++i) {
      dst_pusch_data->cb_present_and_position[i] = src_pusch_data->cb_present_and_position[i];
    }
  }
  if (src->pdu_bit_map & PUSCH_PDU_BITMAP_PUSCH_UCI) {
    const nfapi_nr_pusch_uci_t *src_pusch_uci = &src->pusch_uci;
    nfapi_nr_pusch_uci_t *dst_pusch_uci = &dst->pusch_uci;
    dst_pusch_uci->harq_ack_bit_length = src_pusch_uci->harq_ack_bit_length;
    dst_pusch_uci->csi_part1_bit_length = src_pusch_uci->csi_part1_bit_length;
    dst_pusch_uci->csi_part2_bit_length = src_pusch_uci->csi_part2_bit_length;
    dst_pusch_uci->alpha_scaling = src_pusch_uci->alpha_scaling;
    dst_pusch_uci->beta_offset_harq_ack = src_pusch_uci->beta_offset_harq_ack;
    dst_pusch_uci->beta_offset_csi1 = src_pusch_uci->beta_offset_csi1;
    dst_pusch_uci->beta_offset_csi2 = src_pusch_uci->beta_offset_csi2;
  }
  if (src->pdu_bit_map & PUSCH_PDU_BITMAP_PUSCH_PTRS) {
    const nfapi_nr_pusch_ptrs_t *src_pusch_ptrs = &src->pusch_ptrs;
    nfapi_nr_pusch_ptrs_t *dst_pusch_ptrs = &dst->pusch_ptrs;

    dst_pusch_ptrs->num_ptrs_ports = src_pusch_ptrs->num_ptrs_ports;
    dst_pusch_ptrs->ptrs_ports_list = calloc(dst_pusch_ptrs->num_ptrs_ports, sizeof(nfapi_nr_ptrs_ports_t));
    for (int i = 0; i < src_pusch_ptrs->num_ptrs_ports; ++i) {
      const nfapi_nr_ptrs_ports_t *src_ptrs_port = &src_pusch_ptrs->ptrs_ports_list[i];
      nfapi_nr_ptrs_ports_t *dst_ptrs_port = &dst_pusch_ptrs->ptrs_ports_list[i];

      dst_ptrs_port->ptrs_port_index = src_ptrs_port->ptrs_port_index;
      dst_ptrs_port->ptrs_dmrs_port = src_ptrs_port->ptrs_dmrs_port;
      dst_ptrs_port->ptrs_re_offset = src_ptrs_port->ptrs_re_offset;
    }

    dst_pusch_ptrs->ptrs_time_density = src_pusch_ptrs->ptrs_time_density;
    dst_pusch_ptrs->ptrs_freq_density = src_pusch_ptrs->ptrs_freq_density;
    dst_pusch_ptrs->ul_ptrs_power = src_pusch_ptrs->ul_ptrs_power;
  }
  if (src->pdu_bit_map & PUSCH_PDU_BITMAP_DFTS_OFDM) {
    const nfapi_nr_dfts_ofdm_t *src_dfts_ofdm = &src->dfts_ofdm;
    nfapi_nr_dfts_ofdm_t *dst_dfts_ofdm = &dst->dfts_ofdm;

    dst_dfts_ofdm->low_papr_group_number = src_dfts_ofdm->low_papr_group_number;
    dst_dfts_ofdm->low_papr_sequence_number = src_dfts_ofdm->low_papr_sequence_number;
    dst_dfts_ofdm->ul_ptrs_sample_density = src_dfts_ofdm->ul_ptrs_sample_density;
    dst_dfts_ofdm->ul_ptrs_time_density_transform_precoding = src_dfts_ofdm->ul_ptrs_time_density_transform_precoding;
  }
  copy_ul_tti_beamforming(&src->beamforming, &dst->beamforming);
  dst->maintenance_parms_v3.ldpcBaseGraph = src->maintenance_parms_v3.ldpcBaseGraph;
  dst->maintenance_parms_v3.tbSizeLbrmBytes = src->maintenance_parms_v3.tbSizeLbrmBytes;
}

static void copy_ul_tti_request_pucch_pdu(const nfapi_nr_pucch_pdu_t *src, nfapi_nr_pucch_pdu_t *dst)
{
  dst->rnti = src->rnti;
  dst->handle = src->handle;
  dst->bwp_size = src->bwp_size;
  dst->bwp_start = src->bwp_start;
  dst->subcarrier_spacing = src->subcarrier_spacing;
  dst->cyclic_prefix = src->cyclic_prefix;
  dst->format_type = src->format_type;
  dst->multi_slot_tx_indicator = src->multi_slot_tx_indicator;
  dst->pi_2bpsk = src->pi_2bpsk;
  dst->prb_start = src->prb_start;
  dst->prb_size = src->prb_size;
  dst->start_symbol_index = src->start_symbol_index;
  dst->nr_of_symbols = src->nr_of_symbols;
  dst->freq_hop_flag = src->freq_hop_flag;
  dst->second_hop_prb = src->second_hop_prb;
  dst->group_hop_flag = src->group_hop_flag;
  dst->sequence_hop_flag = src->sequence_hop_flag;
  dst->hopping_id = src->hopping_id;
  dst->initial_cyclic_shift = src->initial_cyclic_shift;
  dst->data_scrambling_id = src->data_scrambling_id;
  dst->time_domain_occ_idx = src->time_domain_occ_idx;
  dst->pre_dft_occ_idx = src->pre_dft_occ_idx;
  dst->pre_dft_occ_len = src->pre_dft_occ_len;
  dst->add_dmrs_flag = src->add_dmrs_flag;
  dst->dmrs_scrambling_id = src->dmrs_scrambling_id;
  dst->dmrs_cyclic_shift = src->dmrs_cyclic_shift;
  dst->sr_flag = src->sr_flag;
  dst->bit_len_harq = src->bit_len_harq;
  dst->bit_len_csi_part1 = src->bit_len_csi_part1;
  dst->bit_len_csi_part2 = src->bit_len_csi_part2;
  copy_ul_tti_beamforming(&src->beamforming, &dst->beamforming);
}

static void copy_ul_tti_request_srs_parameters(const nfapi_v4_srs_parameters_t *src,
                                               const uint8_t num_symbols,
                                               nfapi_v4_srs_parameters_t *dst)
{
  dst->srs_bandwidth_size = src->srs_bandwidth_size;
  for (int symbol_idx = 0; symbol_idx < num_symbols; ++symbol_idx) {
    nfapi_v4_srs_parameters_symbols_t *dst_symbol = &dst->symbol_list[symbol_idx];
    const nfapi_v4_srs_parameters_symbols_t *src_symbol = &src->symbol_list[symbol_idx];
    dst_symbol->srs_bandwidth_start = src_symbol->srs_bandwidth_start;
    dst_symbol->sequence_group = src_symbol->sequence_group;
    dst_symbol->sequence_number = src_symbol->sequence_number;
  }

#ifdef ENABLE_AERIAL
  // For Aerial, we always process the 4 reported symbols, not only the ones indicated by num_symbols
  for (int symbol_idx = num_symbols; symbol_idx < 4; ++symbol_idx) {
    nfapi_v4_srs_parameters_symbols_t *dst_symbol = &dst->symbol_list[symbol_idx];
    const nfapi_v4_srs_parameters_symbols_t *src_symbol = &src->symbol_list[symbol_idx];
    dst_symbol->srs_bandwidth_start = src_symbol->srs_bandwidth_start;
    dst_symbol->sequence_group = src_symbol->sequence_group;
    dst_symbol->sequence_number = src_symbol->sequence_number;
  }
#endif // ENABLE_AERIAL
  dst->usage = src->usage;
  const uint8_t nUsage = __builtin_popcount(dst->usage);
  for (int idx = 0; idx < nUsage; ++idx) {
    dst->report_type[idx] = src->report_type[idx];
  }
  dst->singular_Value_representation = src->singular_Value_representation;
  dst->iq_representation = src->iq_representation;
  dst->prg_size = src->prg_size;
  dst->num_total_ue_antennas = src->num_total_ue_antennas;
  dst->ue_antennas_in_this_srs_resource_set = src->ue_antennas_in_this_srs_resource_set;
  dst->sampled_ue_antennas = src->sampled_ue_antennas;
  dst->report_scope = src->report_scope;
  dst->num_ul_spatial_streams_ports = src->num_ul_spatial_streams_ports;
  for (int idx = 0; idx < dst->num_ul_spatial_streams_ports; ++idx) {
    dst->Ul_spatial_stream_ports[idx] = src->Ul_spatial_stream_ports[idx];
  }
}

static void copy_ul_tti_request_srs_pdu(const nfapi_nr_srs_pdu_t *src, nfapi_nr_srs_pdu_t *dst)
{
  dst->rnti = src->rnti;
  dst->handle = src->handle;
  dst->bwp_size = src->bwp_size;
  dst->bwp_start = src->bwp_start;
  dst->subcarrier_spacing = src->subcarrier_spacing;
  dst->cyclic_prefix = src->cyclic_prefix;
  dst->num_ant_ports = src->num_ant_ports;
  dst->num_symbols = src->num_symbols;
  dst->num_repetitions = src->num_repetitions;
  dst->time_start_position = src->time_start_position;
  dst->config_index = src->config_index;
  dst->sequence_id = src->sequence_id;
  dst->bandwidth_index = src->bandwidth_index;
  dst->comb_size = src->comb_size;
  dst->comb_offset = src->comb_offset;
  dst->cyclic_shift = src->cyclic_shift;
  dst->frequency_position = src->frequency_position;
  dst->frequency_shift = src->frequency_shift;
  dst->frequency_hopping = src->frequency_hopping;
  dst->group_or_sequence_hopping = src->group_or_sequence_hopping;
  dst->resource_type = src->resource_type;
  dst->t_srs = src->t_srs;
  dst->t_offset = src->t_offset;
  copy_ul_tti_beamforming(&src->beamforming, &dst->beamforming);
  copy_ul_tti_request_srs_parameters(&src->srs_parameters_v4, 1 << src->num_symbols, &dst->srs_parameters_v4);
}

void copy_ul_tti_request(const nfapi_nr_ul_tti_request_t *src, nfapi_nr_ul_tti_request_t *dst)
{
  dst->header.message_id = src->header.message_id;
  dst->header.message_length = src->header.message_length;

  dst->SFN = src->SFN;
  dst->Slot = src->Slot;
  dst->n_pdus = src->n_pdus;
  dst->rach_present = src->rach_present;
  dst->n_ulsch = src->n_ulsch;
  dst->n_ulcch = src->n_ulcch;
  dst->n_group = src->n_group;

  for (int PDU = 0; PDU < src->n_pdus; ++PDU) {
    // take the PDU at the start of loops
    const nfapi_nr_ul_tti_request_number_of_pdus_t *src_pdu = &src->pdus_list[PDU];
    nfapi_nr_ul_tti_request_number_of_pdus_t *dst_pdu = &dst->pdus_list[PDU];

    dst_pdu->pdu_type = src_pdu->pdu_type;
    dst_pdu->pdu_size = src_pdu->pdu_size;

    switch (src_pdu->pdu_type) {
      case NFAPI_NR_UL_CONFIG_PRACH_PDU_TYPE:
        copy_ul_tti_request_prach_pdu(&src_pdu->prach_pdu, &dst_pdu->prach_pdu);
        break;
      case NFAPI_NR_UL_CONFIG_PUSCH_PDU_TYPE:
        copy_ul_tti_request_pusch_pdu(&src_pdu->pusch_pdu, &dst_pdu->pusch_pdu);
        break;
      case NFAPI_NR_UL_CONFIG_PUCCH_PDU_TYPE:
        copy_ul_tti_request_pucch_pdu(&src_pdu->pucch_pdu, &dst_pdu->pucch_pdu);
        break;
      case NFAPI_NR_UL_CONFIG_SRS_PDU_TYPE:
        copy_ul_tti_request_srs_pdu(&src_pdu->srs_pdu, &dst_pdu->srs_pdu);
        break;
      default:
        // PDU Type is not any known value
        AssertFatal(1 == 0, "PDU Type value unknown, allowed values range from 0 to 3\n");
    }
  }

  for (int nGroup = 0; nGroup < src->n_group; ++nGroup) {
    dst->groups_list[nGroup].n_ue = src->groups_list[nGroup].n_ue;
    for (int UE = 0; UE < src->groups_list[nGroup].n_ue; ++UE) {
      dst->groups_list[nGroup].ue_list[UE].pdu_idx = src->groups_list[nGroup].ue_list[UE].pdu_idx;
    }
  }
}

void copy_slot_indication(const nfapi_nr_slot_indication_scf_t *src, nfapi_nr_slot_indication_scf_t *dst)
{
  dst->header.message_id = src->header.message_id;
  dst->header.message_length = src->header.message_length;

  dst->sfn = src->sfn;
  dst->slot = src->slot;
}

void copy_ul_dci_request_pdu(const nfapi_nr_ul_dci_request_pdus_t *src, nfapi_nr_ul_dci_request_pdus_t *dst)
{
  dst->PDUType = src->PDUType;
  dst->PDUSize = src->PDUSize;

  // Is the same PDU as the DL_TTI.request PDCCH PDU, reuse copy function
  copy_dl_tti_request_pdcch_pdu(&src->pdcch_pdu.pdcch_pdu_rel15, &dst->pdcch_pdu.pdcch_pdu_rel15);
}

void copy_ul_dci_request(const nfapi_nr_ul_dci_request_t *src, nfapi_nr_ul_dci_request_t *dst)
{
  dst->header.message_id = src->header.message_id;
  dst->header.message_length = src->header.message_length;

  dst->SFN = src->SFN;
  dst->Slot = src->Slot;
  dst->numPdus = src->numPdus;

  for (int pdu_idx = 0; pdu_idx < src->numPdus; ++pdu_idx) {
    copy_ul_dci_request_pdu(&src->ul_dci_pdu_list[pdu_idx], &dst->ul_dci_pdu_list[pdu_idx]);
  }
}
