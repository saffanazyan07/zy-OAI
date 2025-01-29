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

#include "get-yang.h"
#include "common/utils/assertions.h"
#include "common/utils/nr/nr_common.h"

int load_yang_models(struct ly_ctx **ctx)
{
  const char *yang_dir = YANG_MODELS;
  const char *yang_models[] = {"ietf-interfaces", "iana-if-type", "ietf-ip", "iana-hardware", "ietf-hardware", "o-ran-interfaces", "o-ran-module-cap", "o-ran-compression-factors", "o-ran-processing-element", "o-ran-uplane-conf"};

  LY_ERR ret = ly_ctx_new(yang_dir, 0, ctx);
  AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Unable to create libyang context: %s.\n", ly_errmsg(*ctx));

  size_t num_models = sizeof(yang_models) / sizeof(yang_models[0]);
  for (size_t i = 0; i < num_models; ++i) {
    const struct lys_module *mod = ly_ctx_load_module(*ctx, yang_models[i], NULL, NULL);
    AssertError(mod != NULL, return EXIT_FAILURE, "[MPLANE] Failed to load yang model %s.\n", yang_models[i]);
  }

  LOG_I(HW, "[MPLANE] Successfully loaded all yang modules.\n");

  return EXIT_SUCCESS;
}

static int create_cu_interface(struct ly_ctx **ctx, const ru_session_t *ru_session, struct lyd_node **root)
{
  LY_ERR ret = LY_SUCCESS;

  ret = lyd_new_path(NULL, *ctx, "/ietf-interfaces:interfaces", NULL, 0, root);
  AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create root \"interfaces\" node.\n");

  struct lyd_node *interface_node = NULL;
  ret = lyd_new_list(*root, NULL, "interface", 0, &interface_node, "CU_PLANE_INTERFACE");
  AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create \"interface\" node.\n");

  struct lyd_node *type_node = NULL;
  ret = lyd_new_term(interface_node, NULL, "type", "iana-if-type:l2vlan", 0, &type_node);
  AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create \"type\" node.\n");

  ret = lyd_new_term(interface_node, NULL, "enabled", "true", 0, NULL);
  AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create \"enabled\" node.\n");

  const struct lys_module *oran_int_mod = ly_ctx_get_module_implemented(*ctx, "o-ran-interfaces");
  AssertError(oran_int_mod != NULL, return EXIT_FAILURE, "[MPLANE] Failed to get \"o-ran-interfaces\" module.\n");

  struct lyd_node *base_int = NULL;
  ret = lyd_new_term(interface_node, oran_int_mod, "base-interface", ru_session->ru_mplane_config.interface_name, 0, &base_int);
  AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create \"base-interface\" node with name \"%s\".\n", ru_session->ru_mplane_config.interface_name);

  struct lyd_node *vlan_id = NULL;
  ret = lyd_new_term(interface_node, oran_int_mod, "vlan-id", ru_session->ru_mplane_config.vlan_tag[0], 0, &vlan_id); // assuming only 1 ethernet interface is allowed for both CU-planes
  AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create \"vlan-id\" node with \"%s\".\n", ru_session->ru_mplane_config.vlan_tag[0]);

  struct lyd_node *ru_mac_addr = NULL;
  ret = lyd_new_term(interface_node, oran_int_mod, "mac-address", ru_session->xran_mplane.ru_mac_addr, 0, &ru_mac_addr);
  AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create \"mac-address\" node with \"%s\".\n", ru_session->xran_mplane.ru_mac_addr);

  return EXIT_SUCCESS;
}

static int create_proc_elem(struct ly_ctx **ctx, const ru_session_t *ru_session, struct lyd_node **root)
{
  LY_ERR ret = LY_SUCCESS;

  ret = lyd_new_path(NULL, *ctx, "/o-ran-processing-element:processing-elements", NULL, 0, root);
  AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create root \"processing-elements\" node.\n");

  ret = lyd_new_term(*root, NULL, "transport-session-type", "ETH-INTERFACE", 0, NULL);
  AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create \"transport-session-type\" node.\n");

  struct lyd_node *ru_elem_node = NULL;
  ret = lyd_new_list(*root, NULL, "ru-elements", 0, &ru_elem_node, "CUPlaneElement0");
  AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create \"ru-elements\" node.\n");

  struct lyd_node *transp_flow = NULL;
  ret = lyd_new_inner(ru_elem_node, NULL, "transport-flow", 0, &transp_flow);
  AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create \"transport-flow\" node.\n");

  ret = lyd_new_term(transp_flow, NULL, "interface-name", "CU_PLANE_INTERFACE", 0, NULL);
  AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create \"interface-name\" node.\n");

  struct lyd_node *eth_flow = NULL;
  ret = lyd_new_inner(transp_flow, NULL, "eth-flow", 0, &eth_flow);
  AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create \"eth-flow\" node.\n");

  ret = lyd_new_term(eth_flow, NULL, "vlan-id", ru_session->ru_mplane_config.vlan_tag[0], 0, NULL); // assuming only 1 ethernet interface is allowed for both CU-planes
  AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create \"vlan-id\" node.\n");

  ret = lyd_new_term(eth_flow, NULL, "ru-mac-address", ru_session->xran_mplane.ru_mac_addr, 0, NULL);
  AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create \"ru-mac-address\" node.\n");

  ret = lyd_new_term(eth_flow, NULL, "o-du-mac-address", ru_session->ru_mplane_config.du_mac_addr[0], 0, NULL); // assuming only 1 ethernet interface is allowed for both CU-planes
  AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create \"o-du-mac-address\" node.\n");

  return EXIT_SUCCESS;
}

static int fill_uplane_ch(const xran_mplane_t *xran_mplane, const size_t idx, struct lyd_node **root)
{
  LY_ERR ret = LY_SUCCESS;

  ret = lyd_new_term(*root, NULL, "cp-length", "0", 0, NULL);
  AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create \"cp-length\" node.\n");
  
  ret = lyd_new_term(*root, NULL, "cp-length-other", "0", 0, NULL);
  AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create \"cp-length-other\" node.\n");
    
  // to we actually store this from DU config file?
  ret = lyd_new_term(*root, NULL, "offset-to-absolute-frequency-center", "0", 0, NULL);
  AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create \"offset-to-absolute-frequency-center\" node.\n");
  
  struct lyd_node *compression_node = NULL;
  ret = lyd_new_inner(*root, NULL, "compression", 0, &compression_node);
  AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create \"compression\" node.\n");

  char iq_width_str[8];
  sprintf(iq_width_str, "%d", xran_mplane->iq_width);
  ret = lyd_new_term(compression_node, NULL, "iq-bitwidth", iq_width_str, 0, NULL);
  AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create \"iq-bitwidth\" node.\n");
  
  if (xran_mplane->iq_width < 16) {
    ret = lyd_new_term(compression_node, NULL, "compression-type", "STATIC", 0, NULL);
    AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create \"compression-type\" node.\n");
  
    // ret = lyd_new_term(compression_node, NULL, "compression-method", "BLOCK_FLOATING_POINT", 0, NULL);
    // AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create \"compression-method\" node.\n");
  }
    
  struct lyd_node *eaxc_conf = NULL;
  ret = lyd_new_inner(*root, NULL, "e-axcid", 0, &eaxc_conf);
  AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create \"e-axcid\" node.\n");

  char eaxc_id[8];
  sprintf(eaxc_id, "%ld", idx);
  ret = lyd_new_term(eaxc_conf, NULL, "eaxc-id", eaxc_id, 0, NULL);
  AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create \"eaxc-id\" node.\n");
  
  char du_port_bitmask[16];
  sprintf(du_port_bitmask, "%d", xran_mplane->du_port_bitmask);
  ret = lyd_new_term(eaxc_conf, NULL, "o-du-port-bitmask", du_port_bitmask, 0, NULL);
  AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create \"o-du-port-bitmask\" node.\n");
  
  char band_sector_bitmask[8];
  sprintf(band_sector_bitmask, "%d", xran_mplane->band_sector_bitmask);
  ret = lyd_new_term(eaxc_conf, NULL, "band-sector-bitmask", band_sector_bitmask, 0, NULL);
  AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create \"band-sector-bitmask\" node.\n");

  char ccid_bitmask[8];
  sprintf(ccid_bitmask, "%d", xran_mplane->ccid_bitmask);
  ret = lyd_new_term(eaxc_conf, NULL, "ccid-bitmask", ccid_bitmask, 0, NULL);
  AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create \"ccid-bitmask\" node.\n");

  char ru_port_bitmask[8];
  sprintf(ru_port_bitmask, "%d", xran_mplane->ru_port_bitmask);
  ret = lyd_new_term(eaxc_conf, NULL, "ru-port-bitmask", ru_port_bitmask, 0, NULL);
  AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create \"ru-port-bitmask\" node.\n");

  return EXIT_SUCCESS;
}

static int create_uplane_conf(struct ly_ctx **ctx, const ru_session_t *ru_session, const openair0_config_t *oai, const size_t num_rus, struct lyd_node **root)
{
  LY_ERR ret = LY_SUCCESS;

  ret = lyd_new_path(NULL, *ctx, "/o-ran-uplane-conf:user-plane-configuration", NULL, 0, root);
  AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create root \"user-plane-configuration\" node.\n");

  if (ru_session->ru_mplane_config.rx_carriers.num != 1 || ru_session->ru_mplane_config.tx_carriers.num !=1)
    LOG_I(HW, "[MPLANE] Only one carrier supported at the moment.\n");
  
  // RX carriers
  struct lyd_node *rx_carrier_node = NULL;
  ret = lyd_new_list(*root, NULL, "rx-array-carriers", 0, &rx_carrier_node, ru_session->ru_mplane_config.rx_carriers.name[0]);
  AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create \"rx-array-carriers\" node.\n");

  char rx_freq[16];
  sprintf(rx_freq, "%.0f", oai->rx_freq[0]);
  ret = lyd_new_term(rx_carrier_node, NULL, "center-of-channel-bandwidth", rx_freq, 0, NULL);
  AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create \"center-of-channel-bandwidth\" node.\n");

  char rx_arfcn[16];
  sprintf(rx_arfcn, "%d", to_nrarfcn(oai->nr_band, oai->rx_freq[0], oai->nr_scs_for_raster, oai->rx_bw));
  ret = lyd_new_term(rx_carrier_node, NULL, "absolute-frequency-center", rx_arfcn, 0, NULL);
  AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create \"absolute-frequency-center\" node.\n");

  char rx_bw[16];
  sprintf(rx_bw, "%.0f", oai->rx_bw);
  ret = lyd_new_term(rx_carrier_node, NULL, "channel-bandwidth", rx_bw, 0, NULL);
  AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create \"channel-bandwidth\" node.\n");

  ret = lyd_new_term(rx_carrier_node, NULL, "downlink-radio-frame-offset", "0", 0, NULL);
  AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create \"downlink-radio-frame-offset\" node.\n");

  ret = lyd_new_term(rx_carrier_node, NULL, "downlink-sfn-offset", "0", 0, NULL);
  AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create \"downlink-sfn-offset\" node.\n");

  // oai->rx_gain_offset is this the correction? if yes, test it later
  ret = lyd_new_term(rx_carrier_node, NULL, "gain-correction", "0", 0, NULL);
  AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create \"gain-correction\" node.\n");

  ret = lyd_new_term(rx_carrier_node, NULL, "n-ta-offset", "0", 0, NULL);
  AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create \"n-ta-offset\" node.\n");

  ret = lyd_new_term(rx_carrier_node, NULL, "active", "ACTIVE", 0, NULL);
  AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create \"active\" node.\n");

  // TX carriers
  struct lyd_node *tx_carrier_node = NULL;
  ret = lyd_new_list(*root, NULL, "tx-array-carriers", 0, &tx_carrier_node, ru_session->ru_mplane_config.tx_carriers.name[0]);
  AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create \"tx-array-carriers\" node.\n");

  char tx_freq[16];
  sprintf(tx_freq, "%.0f", oai->tx_freq[0]);
  ret = lyd_new_term(tx_carrier_node, NULL, "center-of-channel-bandwidth", tx_freq, 0, NULL);
  AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create \"center-of-channel-bandwidth\" node.\n");

  char tx_arfcn[16];
  sprintf(tx_arfcn, "%d", to_nrarfcn(oai->nr_band, oai->tx_freq[0], oai->nr_scs_for_raster, oai->tx_bw));
  ret = lyd_new_term(tx_carrier_node, NULL, "absolute-frequency-center", tx_arfcn, 0, NULL);
  AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create \"absolute-frequency-center\" node.\n");

  char tx_bw[16];
  sprintf(tx_bw, "%.0f", oai->tx_bw);
  ret = lyd_new_term(tx_carrier_node, NULL, "channel-bandwidth", tx_bw, 0, NULL);
  AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create \"channel-bandwidth\" node.\n");

  // oai->tx_gain to be tested
  ret = lyd_new_term(tx_carrier_node, NULL, "gain", "0", 0, NULL);
  AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create \"gain\" node.\n");
  
  ret = lyd_new_term(tx_carrier_node, NULL, "downlink-radio-frame-offset", "0", 0, NULL);
  AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create \"downlink-radio-frame-offset\" node.\n");

  ret = lyd_new_term(tx_carrier_node, NULL, "downlink-sfn-offset", "0", 0, NULL);
  AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create \"downlink-sfn-offset\" node.\n");

  ret = lyd_new_term(tx_carrier_node, NULL, "active", "ACTIVE", 0, NULL);
  AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create \"active\" node.\n");


  // PUSCH & PRACH endpoints
  const size_t num_rx_ch = oai->rx_num_channels / num_rus;
  for (size_t i = 0; i < num_rx_ch; i++) {
    struct lyd_node *pusch_node = NULL;
    ret = lyd_new_list(*root, NULL, "low-level-rx-endpoints", 0, &pusch_node, ru_session->ru_mplane_config.rx_endpoints.name[i]);
    AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create \"low-level-rx-endpoints\" node.\n");
    
    ret = fill_uplane_ch(&ru_session->xran_mplane, i, &pusch_node);
    AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to fill \"low-level-rx-endpoints\" node for %s.\n", ru_session->ru_mplane_config.rx_endpoints.name[i]);

    const size_t prach_endpoint_name_offset = i + (ru_session->ru_mplane_config.rx_endpoints.num / 2);
    struct lyd_node *prach_node = NULL;
    ret = lyd_new_list(*root, NULL, "low-level-rx-endpoints", 0, &prach_node, ru_session->ru_mplane_config.rx_endpoints.name[prach_endpoint_name_offset]);
    AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create \"low-level-rx-endpoints\" node.\n");

    ret = fill_uplane_ch(&ru_session->xran_mplane, i + ru_session->xran_mplane.prach_offset, &prach_node);
    AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to fill \"low-level-rx-endpoints\" node for %s.\n", ru_session->ru_mplane_config.rx_endpoints.name[prach_endpoint_name_offset]);
  }

  // PDSCH endpoints
  const size_t num_tx_ch = oai->tx_num_channels / num_rus;
  for (size_t i = 0; i < num_tx_ch; i++) {
    struct lyd_node *pdsch_node = NULL;
    ret = lyd_new_list(*root, NULL, "low-level-tx-endpoints", 0, &pdsch_node, ru_session->ru_mplane_config.tx_endpoints.name[i]);
    AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create \"low-level-tx-endpoints\" node.\n");

    ret = fill_uplane_ch(&ru_session->xran_mplane, i, &pdsch_node);
    AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to fill \"low-level-tx-endpoints\" node for %s.\n", ru_session->ru_mplane_config.tx_endpoints.name[i]);
  }



  // PUSCH and PRACH links
  for (size_t i = 0; i < num_rx_ch; i++) {
    // PUSCH
    struct lyd_node *pusch_link_node = NULL;
    char pusch_link[16];
    sprintf(pusch_link, "%s%ld", "PuschLink", i);
    ret = lyd_new_list(*root, NULL, "low-level-rx-links", 0, &pusch_link_node, pusch_link);
    AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create \"low-level-rx-links\" node.\n");

    ret = lyd_new_term(pusch_link_node, NULL, "processing-element", "CUPlaneElement0", 0, NULL);
    AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create \"processing-element\" node.\n");

    ret = lyd_new_term(pusch_link_node, NULL, "rx-array-carrier", ru_session->ru_mplane_config.rx_carriers.name[0], 0, NULL);
    AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create \"rx-array-carrier\" node.\n");

    ret = lyd_new_term(pusch_link_node, NULL, "low-level-rx-endpoint", ru_session->ru_mplane_config.rx_endpoints.name[i], 0, NULL);
    AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create \"low-level-rx-endpoint\" node.\n");

    // PRACH
    struct lyd_node *prach_link_node = NULL;
    char prach_link[16];
    sprintf(prach_link, "%s%ld", "PrachLink", i);
    ret = lyd_new_list(*root, NULL, "low-level-rx-links", 0, &prach_link_node, prach_link);
    AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create \"low-level-rx-links\" node.\n");

    ret = lyd_new_term(prach_link_node, NULL, "processing-element", "CUPlaneElement0", 0, NULL);
    AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create \"processing-element\" node.\n");

    ret = lyd_new_term(prach_link_node, NULL, "rx-array-carrier", ru_session->ru_mplane_config.rx_carriers.name[0], 0, NULL);
    AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create \"rx-array-carrier\" node.\n");

    const size_t prach_endpoint_name_offset = i + (ru_session->ru_mplane_config.rx_endpoints.num / 2);
    ret = lyd_new_term(prach_link_node, NULL, "low-level-rx-endpoint", ru_session->ru_mplane_config.rx_endpoints.name[prach_endpoint_name_offset], 0, NULL);
    AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create \"low-level-rx-endpoint\" node.\n");
  }

  // PDSCH links
  for (size_t i = 0; i < num_tx_ch; i++) {
    struct lyd_node *pdsch_link_node = NULL;
    char pdsch_link[16];
    sprintf(pdsch_link, "%s%ld", "PdschLink", i);
    ret = lyd_new_list(*root, NULL, "low-level-tx-links", 0, &pdsch_link_node, pdsch_link);
    AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create \"low-level-tx-links\" node.\n");

    ret = lyd_new_term(pdsch_link_node, NULL, "processing-element", "CUPlaneElement0", 0, NULL);
    AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create \"processing-element\" node.\n");

    ret = lyd_new_term(pdsch_link_node, NULL, "tx-array-carrier", ru_session->ru_mplane_config.tx_carriers.name[0], 0, NULL);
    AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create \"tx-array-carrier\" node.\n");

    ret = lyd_new_term(pdsch_link_node, NULL, "low-level-tx-endpoint", ru_session->ru_mplane_config.tx_endpoints.name[i], 0, NULL);
    AssertError(ret == LY_SUCCESS, return EXIT_FAILURE, "[MPLANE] Failed to create \"low-level-tx-endpoint\" node.\n");
  }

  return EXIT_SUCCESS;
}

int configure_ru_from_yang(struct ly_ctx **ctx, const ru_session_t *ru_session, const openair0_config_t *oai, const size_t num_rus, char **result)
{
  int ret = 0;

  // <ietf-interfaces>
  struct lyd_node *cu_interface = NULL;
  ret = create_cu_interface(ctx, ru_session, &cu_interface);
  AssertError(ret == 0, return EXIT_FAILURE, "[MPLANE] Cannot create CU-plane interface.\n");

  // <o-ran-processing-element>
  struct lyd_node *proc_elem = NULL;
  ret = create_proc_elem(ctx, ru_session, &proc_elem);
  AssertError(ret == 0, return EXIT_FAILURE, "[MPLANE] Cannot create CU-plane processing element.\n");

  // <o-ran-uplane-conf>
  struct lyd_node *uplane_conf = NULL;
  ret = create_uplane_conf(ctx, ru_session, oai, num_rus, &uplane_conf);
  AssertError(ret == 0, return EXIT_FAILURE, "[MPLANE] Cannot create U-plane configuration.\n");

  ret = lyd_merge_siblings(&proc_elem, cu_interface, 0);
  AssertError(ret == 0, return EXIT_FAILURE, "[MPLANE] Cannot merge CU-plane interface and processing element.\n");
  ret = lyd_merge_siblings(&uplane_conf, proc_elem, 0);
  AssertError(ret == 0, return EXIT_FAILURE, "[MPLANE] Cannot merge CU-plane interface, processing element and U-plane configuration.\n");

  lyd_print_mem(result, uplane_conf, LYD_XML, LYD_PRINT_WITHSIBLINGS);
  LOG_I(HW, "[MPLANE] Sending the following configuration to RU %s:\n%s\n", ru_session->ru_ip_add, *result);

  lyd_free_tree(cu_interface);
  lyd_free_siblings(proc_elem);
  lyd_free_siblings(uplane_conf);

  return EXIT_SUCCESS;
}
