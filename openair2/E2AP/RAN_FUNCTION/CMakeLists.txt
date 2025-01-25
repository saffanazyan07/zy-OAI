set(SM_3GPP_COMMON_ASN_SRCS
    ../flexric/src/sm/rc_sm/ie/asn/AMFPointer.c
    ../flexric/src/sm/rc_sm/ie/asn/AMFRegionID.c
    ../flexric/src/sm/rc_sm/ie/asn/AMFSetID.c
    ../flexric/src/sm/rc_sm/ie/asn/AMF-UE-NGAP-ID.c
    ../flexric/src/sm/rc_sm/ie/asn/asn_application.c
    ../flexric/src/sm/rc_sm/ie/asn/asn_bit_data.c
    ../flexric/src/sm/rc_sm/ie/asn/asn_codecs_prim.c
    ../flexric/src/sm/rc_sm/ie/asn/asn_internal.c
    ../flexric/src/sm/rc_sm/ie/asn/asn_random_fill.c
    ../flexric/src/sm/rc_sm/ie/asn/asn_SEQUENCE_OF.c
    ../flexric/src/sm/rc_sm/ie/asn/asn_SET_OF.c
    ../flexric/src/sm/rc_sm/ie/asn/BIT_STRING.c
    ../flexric/src/sm/rc_sm/ie/asn/CGI.c
    ../flexric/src/sm/rc_sm/ie/asn/constraints.c
    ../flexric/src/sm/rc_sm/ie/asn/constr_SEQUENCE.c
    ../flexric/src/sm/rc_sm/ie/asn/constr_SEQUENCE_OF.c
    ../flexric/src/sm/rc_sm/ie/asn/constr_SET_OF.c
    ../flexric/src/sm/rc_sm/ie/asn/constr_TYPE.c
    ../flexric/src/sm/rc_sm/ie/asn/CoreCPID.c
    ../flexric/src/sm/rc_sm/ie/asn/ENB-ID.c
    ../flexric/src/sm/rc_sm/ie/asn/ENB-UE-X2AP-ID.c
    ../flexric/src/sm/rc_sm/ie/asn/ENB-UE-X2AP-ID-Extension.c
    ../flexric/src/sm/rc_sm/ie/asn/EN-GNB-ID.c
    ../flexric/src/sm/rc_sm/ie/asn/E-UTRA-ARFCN.c
    ../flexric/src/sm/rc_sm/ie/asn/EUTRACellIdentity.c
    ../flexric/src/sm/rc_sm/ie/asn/EUTRA-CGI.c
    ../flexric/src/sm/rc_sm/ie/asn/E-UTRA-PCI.c
    ../flexric/src/sm/rc_sm/ie/asn/E-UTRA-TAC.c
    ../flexric/src/sm/rc_sm/ie/asn/FiveGS-TAC.c
    ../flexric/src/sm/rc_sm/ie/asn/FiveQI.c
    ../flexric/src/sm/rc_sm/ie/asn/FreqBandNrItem.c
    ../flexric/src/sm/rc_sm/ie/asn/GlobalENB-ID.c
    ../flexric/src/sm/rc_sm/ie/asn/GlobalenGNB-ID.c
    ../flexric/src/sm/rc_sm/ie/asn/GlobalGNB-ID.c
    ../flexric/src/sm/rc_sm/ie/asn/GlobalNgENB-ID.c
    ../flexric/src/sm/rc_sm/ie/asn/GlobalNGRANNodeID.c
    ../flexric/src/sm/rc_sm/ie/asn/GNB-CU-CP-UE-E1AP-ID.c
    ../flexric/src/sm/rc_sm/ie/asn/GNB-CU-UE-F1AP-ID.c
    ../flexric/src/sm/rc_sm/ie/asn/GNB-CU-UP-ID.c
    ../flexric/src/sm/rc_sm/ie/asn/GNB-DU-ID.c
    ../flexric/src/sm/rc_sm/ie/asn/GNB-ID.c
    ../flexric/src/sm/rc_sm/ie/asn/GroupID.c
    ../flexric/src/sm/rc_sm/ie/asn/GUAMI.c
    ../flexric/src/sm/rc_sm/ie/asn/GUMMEI.c
    ../flexric/src/sm/rc_sm/ie/asn/IndexToRFSP.c
    ../flexric/src/sm/rc_sm/ie/asn/INTEGER.c
    ../flexric/src/sm/rc_sm/ie/asn/InterfaceID-E1.c
    ../flexric/src/sm/rc_sm/ie/asn/InterfaceIdentifier.c
    ../flexric/src/sm/rc_sm/ie/asn/InterfaceID-F1.c
    ../flexric/src/sm/rc_sm/ie/asn/InterfaceID-NG.c
    ../flexric/src/sm/rc_sm/ie/asn/InterfaceID-S1.c
    ../flexric/src/sm/rc_sm/ie/asn/InterfaceID-W1.c
    ../flexric/src/sm/rc_sm/ie/asn/InterfaceID-X2.c
    ../flexric/src/sm/rc_sm/ie/asn/InterfaceID-Xn.c
    ../flexric/src/sm/rc_sm/ie/asn/Interface-MessageID.c
    ../flexric/src/sm/rc_sm/ie/asn/InterfaceType.c
    ../flexric/src/sm/rc_sm/ie/asn/MME-Code.c
    ../flexric/src/sm/rc_sm/ie/asn/MME-Group-ID.c
    ../flexric/src/sm/rc_sm/ie/asn/MME-UE-S1AP-ID.c
    ../flexric/src/sm/rc_sm/ie/asn/NativeEnumerated.c
    ../flexric/src/sm/rc_sm/ie/asn/NGENB-CU-UE-W1AP-ID.c
    ../flexric/src/sm/rc_sm/ie/asn/NGENB-DU-ID.c
    ../flexric/src/sm/rc_sm/ie/asn/NgENB-ID.c
    ../flexric/src/sm/rc_sm/ie/asn/NG-RANnodeUEXnAPID.c
    ../flexric/src/sm/rc_sm/ie/asn/NR-ARFCN.c
    ../flexric/src/sm/rc_sm/ie/asn/NRCellIdentity.c
    ../flexric/src/sm/rc_sm/ie/asn/NR-CGI.c
    ../flexric/src/sm/rc_sm/ie/asn/NRFrequencyBandItem.c
    ../flexric/src/sm/rc_sm/ie/asn/NRFrequencyBand-List.c
    ../flexric/src/sm/rc_sm/ie/asn/NRFrequencyInfo.c
    ../flexric/src/sm/rc_sm/ie/asn/NRFrequencyShift7p5khz.c
    ../flexric/src/sm/rc_sm/ie/asn/NR-PCI.c
    ../flexric/src/sm/rc_sm/ie/asn/OPEN_TYPE.c
    ../flexric/src/sm/rc_sm/ie/asn/per_decoder.c
    ../flexric/src/sm/rc_sm/ie/asn/per_encoder.c
    ../flexric/src/sm/rc_sm/ie/asn/per_opentype.c
    ../flexric/src/sm/rc_sm/ie/asn/per_support.c
    ../flexric/src/sm/rc_sm/ie/asn/PLMNIdentity.c
    ../flexric/src/sm/rc_sm/ie/asn/QCI.c
    ../flexric/src/sm/rc_sm/ie/asn/QosFlowIdentifier.c
    ../flexric/src/sm/rc_sm/ie/asn/QoSID.c
    ../flexric/src/sm/rc_sm/ie/asn/RANUEID.c
    ../flexric/src/sm/rc_sm/ie/asn/RRCclass-LTE.c
    ../flexric/src/sm/rc_sm/ie/asn/RRCclass-NR.c
    ../flexric/src/sm/rc_sm/ie/asn/RRC-MessageID.c
    ../flexric/src/sm/rc_sm/ie/asn/SD.c
    ../flexric/src/sm/rc_sm/ie/asn/ServingCell-ARFCN.c
    ../flexric/src/sm/rc_sm/ie/asn/ServingCell-PCI.c
    ../flexric/src/sm/rc_sm/ie/asn/S-NSSAI.c
    ../flexric/src/sm/rc_sm/ie/asn/SST.c
    ../flexric/src/sm/rc_sm/ie/asn/SubscriberProfileIDforRFP.c
    ../flexric/src/sm/rc_sm/ie/asn/SupportedSULBandList.c
    ../flexric/src/sm/rc_sm/ie/asn/SupportedSULFreqBandItem.c
    ../flexric/src/sm/rc_sm/ie/asn/UEID.c
    ../flexric/src/sm/rc_sm/ie/asn/UEID-ENB.c
    ../flexric/src/sm/rc_sm/ie/asn/UEID-EN-GNB.c
    ../flexric/src/sm/rc_sm/ie/asn/UEID-GNB.c
    ../flexric/src/sm/rc_sm/ie/asn/UEID-GNB-CU-CP-E1AP-ID-Item.c
    ../flexric/src/sm/rc_sm/ie/asn/UEID-GNB-CU-CP-E1AP-ID-List.c
    ../flexric/src/sm/rc_sm/ie/asn/UEID-GNB-CU-CP-F1AP-ID-Item.c
    ../flexric/src/sm/rc_sm/ie/asn/UEID-GNB-CU-F1AP-ID-List.c
    ../flexric/src/sm/rc_sm/ie/asn/UEID-GNB-CU-UP.c
    ../flexric/src/sm/rc_sm/ie/asn/UEID-GNB-DU.c
    ../flexric/src/sm/rc_sm/ie/asn/UEID-NG-ENB.c
    ../flexric/src/sm/rc_sm/ie/asn/UEID-NG-ENB-DU.c
)


add_library(e2_ran_func_cuup STATIC
            init_ran_func.c
            read_setup_ran.c
            O-RAN/ran_e2sm_ue_id.c
            O-RAN/ran_func_kpm.c
            O-RAN/ran_func_kpm_subs.c
            O-RAN/ran_func_rc.c
            O-RAN/ran_func_rc_subs.c
            CUSTOMIZED/ran_func_gtp.c    # GTP SM not yet implemented in CU-UP
            CUSTOMIZED/ran_func_pdcp.c
            CUSTOMIZED/ran_func_tc.c     # currently, not implemented; therefore, filling rnd data
            ../flexric/test/rnd/fill_rnd_data_tc.c
            ../flexric/src/lib/sm/enc/enc_ue_id.c
            ${SM_3GPP_COMMON_ASN_SRCS}
            )

target_link_libraries(e2_ran_func_cuup PUBLIC asn1_nr_rrc nr_rrc asn1_nr_rrc_hdrs e2_time_obj kpm_ric_info_common_obj 3gpp_derived_ie_obj sm_common_ie_obj 3gpp_derived_ie_enc_asn_obj_kpm)
target_compile_definitions(e2_ran_func_cuup PUBLIC ${E2AP_VERSION}  ${KPM_VERSION}  NGRAN_GNB_CUUP)


add_library(e2_ran_func_du_cucp_cuup STATIC
            init_ran_func.c
            read_setup_ran.c
            O-RAN/ran_e2sm_ue_id.c
            O-RAN/ran_func_kpm.c
            O-RAN/ran_func_kpm_subs.c
            O-RAN/ran_func_rc.c
            O-RAN/ran_func_rc_subs.c
            ../flexric/src/sm/rc_sm/ie/rc_data_ie.c
            CUSTOMIZED/ran_func_gtp.c
            CUSTOMIZED/ran_func_pdcp.c
            CUSTOMIZED/ran_func_mac.c
            CUSTOMIZED/ran_func_rlc.c
            CUSTOMIZED/ran_func_slice.c  # currently, not implemented; therefore, filling rnd data
            ../flexric/test/rnd/fill_rnd_data_slice.c
            CUSTOMIZED/ran_func_tc.c     # currently, not implemented; therefore, filling rnd data
            ../flexric/test/rnd/fill_rnd_data_tc.c
            ../flexric/src/lib/sm/enc/enc_ue_id.c
            ${SM_3GPP_COMMON_ASN_SRCS}
            )

target_link_libraries(e2_ran_func_du_cucp_cuup PUBLIC asn1_nr_rrc nr_rrc asn1_nr_rrc_hdrs e2_time_obj kpm_ric_info_common_obj 3gpp_derived_ie_obj e2sm_rc_ir_obj sm_common_ie_obj 3gpp_derived_ie_enc_asn_obj_kpm)
target_compile_definitions(e2_ran_func_du_cucp_cuup PUBLIC ${E2AP_VERSION}  ${KPM_VERSION} NGRAN_GNB_DU NGRAN_GNB_CUCP NGRAN_GNB_CUUP)

# Current implementation:
# - we only have one executable nr-softmodem for 4 node types: DU, CU, gNB-mono, CU-CP

# In the future, when nr-softmodem is separated, linking should be done as following:
# - DU: e2_ran_func_du
# - CU-CP : e2_ran_func_cucp
# - CU-UP: e2_ran_func_cuup (this is available at the moment, executable nr-cuup)
# - CU: e2_ran_func_cucp + e2_ran_func_cuup
# - gNB-mono: e2_ran_func_du + e2_ran_func_cucp + e2_ran_func_cuup
