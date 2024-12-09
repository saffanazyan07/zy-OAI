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

#ifndef RAN_FUNC_SM_RAN_CTRL_SUBSCRIPTION_AGENT_H
#define RAN_FUNC_SM_RAN_CTRL_SUBSCRIPTION_AGENT_H

#include "common/utils/hashtable/hashtable.h"
#include "common/utils/collection/tree.h"

typedef enum {    // 8.2.1 RAN Parameters for Report Service Style 1
  UE_EVENT_ID_E2SM_RC_RAN_PARAM_ID_REPORT_1 = 1,
  NI_MESSAGE_E2SM_RC_RAN_PARAM_ID_REPORT_1 = 2,
  RRC_MESSAGE_E2SM_RC_RAN_PARAM_ID_REPORT_1 = 3,
  UE_ID_E2SM_RC_RAN_PARAM_ID_REPORT_1 = 4,
  OLD_AMF_UE_NGAP_ID_E2SM_RC_RAN_PARAM_ID_REPORT_1 = 5,
  CELL_GLOBAL_ID_E2SM_RC_RAN_PARAM_ID_REPORT_1 = 6,

  END_E2SM_RC_REPORT_STYLE_1_RAN_PARAM_ID
} report_style_1_ran_param_id_e;

typedef enum {    // 8.2.2 RAN Parameters for Report Service Style 2
  CURRENT_UE_ID_E2SM_RC_RAN_PARAM_ID_REPORT_2 = 1,
  OLD_UE_ID_E2SM_RC_RAN_PARAM_ID_REPORT_2 = 2,
  CURRENT_RRC_STATE_E2SM_RC_RAN_PARAM_ID_REPORT_2 = 3,
  OLD_RRC_STATE_E2SM_RC_RAN_PARAM_ID_REPORT_2 = 4,
  UE_CONTEXT_INFORMATION_CONTAINER_E2SM_RC_RAN_PARAM_ID_REPORT_2 = 5,
  CELL_GLOBAL_ID_E2SM_RC_RAN_PARAM_ID_REPORT_2 = 6,
  UE_INFORMATION_E2SM_RC_RAN_PARAM_ID_REPORT_2 = 7,

  END_E2SM_RC_REPORT_STYLE_2_RAN_PARAM_ID
} report_style_2_ran_param_id_e;

typedef enum {    // 8.2.3 RAN Parameters for Report Service Style 3
  CELL_CONTEXT_INFORMATION_E2SM_RC_RAN_PARAM_ID_REPORT_3 = 1,
  CELL_DELETED_E2SM_RC_RAN_PARAM_ID_REPORT_3 = 2,
  NEIGHBOUR_RELATION_TABLE_E2SM_RC_RAN_PARAM_ID_REPORT_3 = 3,

  END_E2SM_RC_REPORT_STYLE_3_RAN_PARAM_ID
} report_style_3_ran_param_id_e;

typedef enum {    // 8.2.4 RAN Parameters for Report Service Style 4
  UL_MAC_CE_E2SM_RC_RAN_PARAM_ID_REPORT_4 = 100,
  DL_MAC_CE_E2SM_RC_RAN_PARAM_ID_REPORT_4 = 101,
  DL_BUFFER_OCCUPANCY_E2SM_RC_RAN_PARAM_ID_REPORT_4 = 102,
  CURRENT_RRC_STATE_E2SM_RC_RAN_PARAM_ID_REPORT_4 = 201,
  RRC_STATE_CHANGED_TO_E2SM_RC_RAN_PARAM_ID_REPORT_4 = 202,
  RRC_MESSAGE_E2SM_RC_RAN_PARAM_ID_REPORT_4 = 203,
  OLD_UE_ID_E2SM_RC_RAN_PARAM_ID_REPORT_4 = 300,
  CURRENT_UE_ID_E2SM_RC_RAN_PARAM_ID_REPORT_4 = 301,
  NI_MESSAGE_E2SM_RC_RAN_PARAM_ID_REPORT_4 = 302,
  CELL_GLOBAL_ID_E2SM_RC_RAN_PARAM_ID_REPORT_4 = 400,

  END_E2SM_RC_REPORT_STYLE_4_RAN_PARAM_ID
} report_style_4_ran_param_id_e;

typedef enum {    // 8.2.5 RAN Parameters for Report Service Style 5
  UE_CONTEXT_INFORMATION_E2SM_RC_RAN_PARAM_ID_REPORT_5 = 1,
  CELL_CONTEXT_INFORMATION_E2SM_RC_RAN_PARAM_ID_REPORT_5 = 2,
  NEIGHBOUR_RELATION_TABLE_E2SM_RC_RAN_PARAM_ID_REPORT_5 = 3,

  END_E2SM_RC_REPORT_STYLE_5_RAN_PARAM_ID
} report_style_5_ran_param_id_e;

typedef enum {
  RC_REPORT_STYLE_1 = 1,
  RC_REPORT_STYLE_2 = 2,
  RC_REPORT_STYLE_3 = 3,
  RC_REPORT_STYLE_4 = 4,
  RC_REPORT_STYLE_5 = 5,
} report_style_e;
typedef struct{
  size_t len;
  union {
    report_style_1_ran_param_id_e* sty1_ran_param_id;
    report_style_2_ran_param_id_e* sty2_ran_param_id;
    report_style_3_ran_param_id_e* sty3_ran_param_id;
    report_style_4_ran_param_id_e* sty4_ran_param_id;
    report_style_5_ran_param_id_e* sty5_ran_param_id;
  };
} arr_ran_param_id_t;

typedef struct ric_req_id_s {
  RB_ENTRY(ric_req_id_s) entries;
  uint32_t ric_req_id;
} rb_ric_req_id_t;

RB_HEAD(ric_id_2_param_id_trees, ric_req_id_s); // Declare ric_id_2_param_id_trees struct once
typedef struct {
  union {
    struct ric_id_2_param_id_trees rb_1[END_E2SM_RC_REPORT_STYLE_1_RAN_PARAM_ID];  //  1 RB tree = (1 RAN Parameter ID) : (n RIC Request ID) => m RB tree = (m RAN Parameter ID) : (n RIC Request ID)
    struct ric_id_2_param_id_trees rb_2[END_E2SM_RC_REPORT_STYLE_2_RAN_PARAM_ID];
    struct ric_id_2_param_id_trees rb_3[END_E2SM_RC_REPORT_STYLE_3_RAN_PARAM_ID];
    struct ric_id_2_param_id_trees rb_4[END_E2SM_RC_REPORT_STYLE_4_RAN_PARAM_ID];
    struct ric_id_2_param_id_trees rb_5[END_E2SM_RC_REPORT_STYLE_5_RAN_PARAM_ID];
  };

  hash_table_t* htable;    // 1 Hash table = (n RIC Request ID) : (m RAN Parameter ID)
} rc_subs_data_t;


int cmp_ric_req_id(struct ric_req_id_s *c1, struct ric_req_id_s *c2);

void init_rc_subs_data(rc_subs_data_t* rc_subs_data, int rc_report_style);
void insert_rc_subs_data(rc_subs_data_t* rc_subs_data, uint32_t ric_req_id, arr_ran_param_id_t* arr_ran_param_id, int rc_report_style);
void remove_rc_subs_data(rc_subs_data_t* rc_subs_data, uint32_t ric_req_id, int rc_report_style);

#endif
