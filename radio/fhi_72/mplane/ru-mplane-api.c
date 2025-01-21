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

#include "ru-mplane-api.h"
#include "xml/get-xml.h"
#include "common/utils/assertions.h"

#include <string.h>

int get_config_for_xran(const char *buffer, const int max_num_ant, xran_mplane_t *xran_mplane)
{
  /* some O-RU vendors are not fully compliant as per M-plane specifications */
  const char *ru_vendor = (char *)get_ru_xml_node(buffer, "mfg-name");

  // RU MAC
  xran_mplane->ru_mac_addr = (char *)get_ru_xml_node(buffer, "mac-address"); // TODO: support for VVDN, as it defines multiple MAC addresses

  // MTU
  const uint32_t interface_mtu = (uint32_t)atoi((char *)get_ru_xml_node(buffer, "interface-mtu"));

  // IQ bitwidth
  char **match_list = NULL;
  size_t count = 0;
  get_ru_xml_list(buffer, "iq-bitwidth", &match_list, &count);
  const int16_t first_iq_width = (int16_t)atoi((char *)match_list[0]);

  // PRACH offset
  // xran_mplane->prach_offset

  if (strcmp(ru_vendor, "BENETEL") == 0 /* || strcmp(ru_vendor, "VVDN-LPRU") == 0 || strcmp(ru_vendor, "Metanoia") == 0 */) {
    if (interface_mtu == 1500) {
      printf("[MPLANE] Interface MTU %d not reliable, hardcoding to 9600.\n", interface_mtu);
      xran_mplane->mtu = 9600;
    } else {
      xran_mplane->mtu = interface_mtu;
    }

    if (first_iq_width != 9) {
      printf("[MPLANE] IQ bitwidth %d not reliable, hardcoding to 9.\n", first_iq_width);
      xran_mplane->iq_width = 9;
    } else {
      xran_mplane->iq_width = first_iq_width;
    }
  
    xran_mplane->prach_offset = max_num_ant;
  } else {
    AssertError(false, return EXIT_FAILURE, "[MPLANE] %s RU currently not supported.\n", ru_vendor);
  }

  printf("\
  [MPLANE]\
    RU vendor name %s\n\
    RU MAC address %s\n\
    MTU %d\n\
    IQ bitwidth %d\n\
    PRACH offset %d\n",
    ru_vendor,
    xran_mplane->ru_mac_addr,
    xran_mplane->mtu,
    xran_mplane->iq_width,
    xran_mplane->prach_offset);

  return EXIT_SUCCESS;
}
