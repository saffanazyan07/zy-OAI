/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.1 (the "License"); you may not use this file
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

#ifndef FGMM_SEC_MODE_REJECT_H
#define FGMM_SEC_MODE_REJECT_H

#include <stdint.h>
#include "openair3/NAS/COMMON/NR_NAS_defs.h"

typedef struct {
  // 5GMM cause (M)
  cause_id_t cause;
} fgmm_sec_mode_reject_msg_t;

int decode_fgmm_sec_mode_reject(fgmm_sec_mode_reject_msg_t *msg, const uint8_t *buffer, uint32_t len);
int encode_fgmm_sec_mode_reject(uint8_t *buffer, const fgmm_sec_mode_reject_msg_t *msg, uint32_t len);

#endif /* FGMM_SEC_MODE_REJECT_H */
