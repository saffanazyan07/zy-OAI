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

#include "init-mplane.h"
#include "radio/fhi_72/oran-params.h"

#include <libyang/libyang.h>
#include <nc_client.h>

static void lnc2_print_clb(NC_VERB_LEVEL level, const char *msg)
{
  switch (level) {
    case NC_VERB_ERROR:
      AssertFatal(false, "[LIBNETCONF2] ERROR: %s\n", msg);
    case NC_VERB_WARNING:
      fprintf(stderr, "[LIBNETCONF2] WARNING: %s\n", msg);
      break;
    case NC_VERB_VERBOSE:
      fprintf(stderr, "[LIBNETCONF2] VERBOSE: %s\n", msg);
      break;
    case NC_VERB_DEBUG:
    case NC_VERB_DEBUG_LOWLVL:
      fprintf(stderr, "[LIBNETCONF2] DEBUG: %s\n", msg);
      break;
    default:
      assert(false && "[LIBNETCONF2] Unknown log level.");
  }
}

static void ly_print_clb(LY_LOG_LEVEL level, const char *msg, const char *path)
{
  switch (level) {
    case LY_LLERR:
      AssertFatal(false, "[LIBYANG] ERROR: %s (path: %s)\n", msg, path);
    case LY_LLWRN:
      fprintf(stderr, "[LIBYANG] WARNING: %s (path: %s)\n", msg, path);
      break;
    case LY_LLVRB:
      fprintf(stderr, "[LIBYANG] VERBOSE: %s (path: %s)\n", msg, path);
      break;
    case LY_LLDBG:
      fprintf(stderr, "[LIBYANG] DEBUG: %s (path: %s)\n", msg, path);
      break;
    default:
      assert(false && "[LIBYANG] Unknown log level.");
  }
}

static const paramdef_t *gpd(const paramdef_t *pd, int num, const char *name)
{
  /* the config module does not know const-correctness... */
  int idx = config_paramidx_fromname((paramdef_t *)pd, num, (char *)name);
  DevAssert(idx >= 0);
  return &pd[idx];
}

int init_mplane(ru_session_list_t *ru_session_list)
{
  paramdef_t fhip[] = ORAN_GLOBALPARAMS_DESC;
  int nump = sizeofArray(fhip);
  int ret = config_get(config_get_if(), fhip, nump, CONFIG_STRING_ORAN);
  if (ret <= 0) {
    printf("problem reading section \"%s\"\n", CONFIG_STRING_ORAN);
    return false;
  }

  ru_session_list->du_key_pair = gpd(fhip, nump, ORAN_CONFIG_DU_KEYPAIR)->strlistptr;
  int num_keys = gpd(fhip, nump, ORAN_CONFIG_DU_KEYPAIR)->numelt;
  AssertError(num_keys == 2, return EXIT_FAILURE, "[MPLANE] Expected {pub-key-path, priv-key-path}. Loaded {%s, %s}.\n", ru_session_list->du_key_pair[0], ru_session_list->du_key_pair[1]);

  char **ru_ip_addrs = gpd(fhip, nump, ORAN_CONFIG_RU_IP_ADDR)->strlistptr;
  int num_rus = gpd(fhip, nump, ORAN_CONFIG_RU_IP_ADDR)->numelt;
  char **du_mac_addr = gpd(fhip, nump, ORAN_CONFIG_DU_ADDR)->strlistptr;
  int num_dus = gpd(fhip, nump, ORAN_CONFIG_DU_ADDR)->numelt;
  uint32_t *vlan_tag = gpd(fhip, nump, ORAN_FH_CONFIG_VLAN_TAG)->uptr;
  int num_vlan_tags = gpd(fhip, nump, ORAN_FH_CONFIG_VLAN_TAG)->numelt;

  AssertError(num_dus == num_vlan_tags, return EXIT_FAILURE, "Number of DU MAC addresses should be equal to the number of VLAN tags.");
 
  int num_cu_planes = num_dus / num_rus;

  ru_session_list->num_rus = num_rus;
  ru_session_list->ru_session = calloc(num_rus, sizeof(ru_session_t));
  for (size_t i = 0; i < num_rus; i++) {
    ru_session_t *ru_session = &ru_session_list->ru_session[i];
    ru_session->session = NULL;
    ru_session->ru_ip_add = calloc(strlen(ru_ip_addrs[i]) + 1, sizeof(char));
    memcpy(ru_session->ru_ip_add, ru_ip_addrs[i], strlen(ru_ip_addrs[i]) + 1);

    // store DU MAC addresses and VLAN tags
    for (int j = 0; j < num_cu_planes; j++) {
      ru_session->ru_mplane_config.du_mac_addr[j] = calloc(1, strlen(du_mac_addr[i+j]) + 1);
      memcpy(ru_session->ru_mplane_config.du_mac_addr[j], du_mac_addr[i+j], strlen(du_mac_addr[i+j]) + 1);
      ru_session->ru_mplane_config.vlan_tag[j] = vlan_tag[i+j];
    }
  }

  nc_client_init();

  // logs for netconf2 and yang libraries
  nc_set_print_clb(lnc2_print_clb); 
  ly_set_log_clb(ly_print_clb, 1);

  return EXIT_SUCCESS;
}
