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

#ifndef RU_MPLANE_API_H
#define RU_MPLANE_API_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
  char *ru_mac_addr;
  uint8_t mtu;
  uint8_t iq_width;
  uint8_t prach_offset;

} xran_mplane_t;

typedef struct {
  char *du_mac_addr[2]; // [0] -> U-plane; [1] -> C-plane
  uint32_t vlan_tag[2]; // [0] -> U-plane; [1] -> C-plane
  // store BW, SCS,..., from openair0_config_t
  // store interface name, low-level-tx/rx-endpoint names, tx/rx-carrier name,..., from RU operational datastore

} ru_mplane_config_t;

typedef struct {
  char *ru_ip_add;
  ru_mplane_config_t ru_mplane_config;
  void *session;
  xran_mplane_t xran_mplane;

} ru_session_t;

typedef struct {
  size_t num_rus;
  ru_session_t *ru_session;
  char **du_key_pair;

} ru_session_list_t;

#endif /* RU_MPLANE_API_H */
