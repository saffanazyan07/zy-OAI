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

#include "ran_func_rc_subs.h"
#include "common/utils/assertions.h"

#include <assert.h>
#include <pthread.h>

#define MAX_NUM_RIC_REQ_ID 64

static pthread_mutex_t rc_mutex = PTHREAD_MUTEX_INITIALIZER;

int cmp_ric_req_id(struct ric_req_id_s *c1, struct ric_req_id_s *c2)
{
  if (c1->ric_req_id < c2->ric_req_id)
    return -1;

  if (c1->ric_req_id > c2->ric_req_id)
    return 1;

  return 0;
}

RB_GENERATE(ric_id_2_param_id_trees, ric_req_id_s, entries, cmp_ric_req_id);

void init_rc_subs_data(rc_subs_data_t* rc_subs_data, int rc_report_style)
{
  pthread_mutex_lock(&rc_mutex);
 
  // Initialize hash table
  DevAssert(rc_subs_data->htable == NULL);
 
  // Initialize RB trees
  // 1 RB tree = 1 ran_param_id => many ric_req_id(s)
  switch (rc_report_style) {
    case RC_REPORT_STYLE_1:
      for (size_t i = 0; i < END_E2SM_RC_REPORT_STYLE_1_RAN_PARAM_ID; i++) {
        RB_INIT(&rc_subs_data->rb_1[i]);
      }
      break;

    case RC_REPORT_STYLE_2:
      for (size_t i = 0; i < END_E2SM_RC_REPORT_STYLE_2_RAN_PARAM_ID; i++) {
        RB_INIT(&rc_subs_data->rb_2[i]);
      }
      break;

    case RC_REPORT_STYLE_3:
      for (size_t i = 0; i < END_E2SM_RC_REPORT_STYLE_3_RAN_PARAM_ID; i++) {
        RB_INIT(&rc_subs_data->rb_3[i]);
      }
      break;

    case RC_REPORT_STYLE_4:
      for (size_t i = 0; i < END_E2SM_RC_REPORT_STYLE_4_RAN_PARAM_ID; i++) {
        RB_INIT(&rc_subs_data->rb_4[i]);
      }
      break;

    case RC_REPORT_STYLE_5:
      for (size_t i = 0; i < END_E2SM_RC_REPORT_STYLE_5_RAN_PARAM_ID; i++) {
        RB_INIT(&rc_subs_data->rb_5[i]);
      }
      break;

    default:
      printf("Invalid Report Style for RAN Control Service Model. Cannot initialize rc_subs_data!\n");
  }

  rc_subs_data->htable = hashtable_create(MAX_NUM_RIC_REQ_ID, NULL, free);
  assert(rc_subs_data->htable != NULL && "Memory exhausted");
  pthread_mutex_unlock(&rc_mutex);
}

void insert_rc_subs_data(rc_subs_data_t* rc_subs_data, uint32_t ric_req_id, arr_ran_param_id_t* arr_ran_param_id, int rc_report_style)
{
  pthread_mutex_lock(&rc_mutex);

  // Insert in hash table
  DevAssert(rc_subs_data->htable != NULL);
  uint64_t key = ric_req_id;
  // Check if the subscription already exists
  AssertFatal(hashtable_is_key_exists(rc_subs_data->htable, key) == HASH_TABLE_KEY_NOT_EXISTS, "RIC req ID %d already subscribed", ric_req_id);
  arr_ran_param_id_t* data = malloc(sizeof(*data));
  assert(data != NULL);
  *data = *arr_ran_param_id;
  hashtable_rc_t ret = hashtable_insert(rc_subs_data->htable, key, data);
  assert(ret == HASH_TABLE_OK  && "Hash table not ok");

  // Insert in RB trees
  // 1 RB tree = 1 ran_param_id => many ric_req_id(s)
  const size_t sz = arr_ran_param_id->len;
  rb_ric_req_id_t *node = calloc(1, sizeof(*node));
  assert(node != NULL);
  node->ric_req_id = ric_req_id;

  switch (rc_report_style) {
    case RC_REPORT_STYLE_1:
      for (size_t i = 0; i < sz; i++) {
        RB_INSERT(ric_id_2_param_id_trees, &rc_subs_data->rb_1[arr_ran_param_id->sty1_ran_param_id[i]], node);
      }
      break;

    case RC_REPORT_STYLE_2:
      for (size_t i = 0; i < sz; i++) {
        RB_INSERT(ric_id_2_param_id_trees, &rc_subs_data->rb_2[arr_ran_param_id->sty2_ran_param_id[i]], node);
      }
      break;

    case RC_REPORT_STYLE_3:
      for (size_t i = 0; i < sz; i++) {
        RB_INSERT(ric_id_2_param_id_trees, &rc_subs_data->rb_3[arr_ran_param_id->sty3_ran_param_id[i]], node);
      }
      break;

    case RC_REPORT_STYLE_4:
      for (size_t i = 0; i < sz; i++) {
        RB_INSERT(ric_id_2_param_id_trees, &rc_subs_data->rb_4[arr_ran_param_id->sty4_ran_param_id[i]], node);
      }
      break;

    case RC_REPORT_STYLE_5:
      for (size_t i = 0; i < sz; i++) {
        RB_INSERT(ric_id_2_param_id_trees, &rc_subs_data->rb_5[arr_ran_param_id->sty5_ran_param_id[i]], node);
      }
      break;

    default:
      printf("Invalid Report Style for RAN Control Service Model. Cannot insert rc_subs_data!\n");
  }

  pthread_mutex_unlock(&rc_mutex);
}

void remove_rc_subs_data(rc_subs_data_t* rc_subs_data, uint32_t ric_req_id, int rc_report_style)
{
  pthread_mutex_lock(&rc_mutex);
  DevAssert(rc_subs_data->htable != NULL);

  uint64_t key = ric_req_id;
  // Get the array of ran_param_id(s)
  void *data = NULL;
  hashtable_rc_t ret = hashtable_get(rc_subs_data->htable, key, &data);
  AssertFatal(ret == HASH_TABLE_OK && data != NULL, "element for ue_id %d not found\n", ric_req_id);
  arr_ran_param_id_t arr_ran_param_id = *(arr_ran_param_id_t *)data;
  // Remove ric_req_id with its ran_param_id(s) from hash table
  ret = hashtable_remove(rc_subs_data->htable, key);
  
  // Remove ric_req_id from each ran_param_id tree where subscribed
  rb_ric_req_id_t *node = calloc(1, sizeof(*node));
  assert(node != NULL);
  node->ric_req_id = ric_req_id;

  switch (rc_report_style) {
    case RC_REPORT_STYLE_1:
      for (size_t i = 0; i < arr_ran_param_id.len; i++) {
        RB_REMOVE(ric_id_2_param_id_trees, &rc_subs_data->rb_1[arr_ran_param_id.sty1_ran_param_id[i]], node);
      }
      break;

    case RC_REPORT_STYLE_2:
      for (size_t i = 0; i < arr_ran_param_id.len; i++) {
        RB_REMOVE(ric_id_2_param_id_trees, &rc_subs_data->rb_2[arr_ran_param_id.sty2_ran_param_id[i]], node);
      }
      break;

    case RC_REPORT_STYLE_3:
      for (size_t i = 0; i < arr_ran_param_id.len; i++) {
        RB_REMOVE(ric_id_2_param_id_trees, &rc_subs_data->rb_3[arr_ran_param_id.sty3_ran_param_id[i]], node);
      }
      break;

    case RC_REPORT_STYLE_4:
      for (size_t i = 0; i < arr_ran_param_id.len; i++) {
        RB_REMOVE(ric_id_2_param_id_trees, &rc_subs_data->rb_4[arr_ran_param_id.sty4_ran_param_id[i]], node);
      }
      break;

    case RC_REPORT_STYLE_5:
      for (size_t i = 0; i < arr_ran_param_id.len; i++) {
        RB_REMOVE(ric_id_2_param_id_trees, &rc_subs_data->rb_5[arr_ran_param_id.sty5_ran_param_id[i]], node);
      }
      break;

    default:
      printf("Invalid Report Style for RAN Control Service Model. Cannot remove rc_subs_data!\n");
  }

  pthread_mutex_unlock(&rc_mutex);
}
