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

#ifndef COMMON_UTIL_TIME_MANAGER_TIME_MANAGER
#define COMMON_UTIL_TIME_MANAGER_TIME_MANAGER

#include <stdint.h>

typedef enum {
  TIME_MANAGER_GNB_MONOLITHIC,
  TIME_MANAGER_GNB_CU,
  TIME_MANAGER_GNB_DU,
  TIME_MANAGER_UE,
  TIME_MANAGER_SIMULATOR,
} time_manager_client_t;

typedef enum {
  TIME_MANAGER_REALTIME,
  TIME_MANAGER_IQ_SAMPLES
} time_manager_mode_t;

void time_manager_start(time_manager_client_t client_type,
                        time_manager_mode_t running_mode);
void time_manager_iq_samples(uint64_t iq_samples_count,
                             uint64_t iq_samples_per_second);
void time_manager_finish(void);

#endif /* COMMON_UTIL_TIME_MANAGER_TIME_MANAGER */
