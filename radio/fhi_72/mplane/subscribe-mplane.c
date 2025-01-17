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

#include "subscribe-mplane.h"
#include "rpc-send-recv.h"
#include "common/utils/assertions.h"

#include <libyang/libyang.h>
#include <nc_client.h>

#ifdef V1
static void notif_clb_v1(struct nc_session *session, const struct nc_notif *notif)
{
  ru_notif_t *answer = nc_session_get_data(session);
  LYD_FORMAT output_format = LYD_JSON;
  uint32_t output_flag = 0;

  char *subs_reply = NULL;
  lyd_print_mem(&subs_reply, notif->tree, output_format, LYP_WITHSIBLINGS | output_flag);
  printf("\nReceived notification at (%s)\n%s\n", notif->datetime, subs_reply);

  // only for ptp-state-change subscription
  const char *node_name = notif->tree->child->attr->name;
  if (strcmp(node_name, "sync-state")) {
    const char *value = notif->tree->child->attr->value_str;
    if (strcmp(value, "LOCKED") == 0) {
      answer->ptp_state = true;
    } else if (strcmp(value, "FREERUN") == 0 || strcmp(value, "HOLDOVER") == 0) {
      answer->ptp_state = false;
    }
  }
}
#elif V2
static void notif_clb_v2(struct nc_session *session, const struct lyd_node *envp, const struct lyd_node *op, void *user_data)
{
  ru_notif_t *answer = (ru_notif_t *)user_data;
  LYD_FORMAT output_format = LYD_JSON;
  uint32_t output_flag = 0;

  char *subs_reply = NULL;
  lyd_print_mem(&subs_reply, op, output_format, LYD_PRINT_WITHSIBLINGS | output_flag);
  printf("\nReceived notification at (%s)\n%s\n", ((struct lyd_node_opaq *)lyd_child(envp))->value, subs_reply);

  // only for ptp-state-change subscription
  const char *node_name = ((struct lyd_node_inner *)op)->child->schema->name;
  if (strcmp(node_name, "sync-state")) {
    const char *value = lyd_get_value(((struct lyd_node_inner *)op)->child);
    if (strcmp(value, "LOCKED") == 0) {
      answer->ptp_state = true;
    } else if (strcmp(value, "FREERUN") == 0 || strcmp(value, "HOLDOVER") == 0) {
      answer->ptp_state = false;
    }
  }
}
#endif

int subscribe_mplane(ru_session_t *ru_session, const char *stream, const char *filter, void *answer)
{
  int timeout = CLI_RPC_REPLY_TIMEOUT;
  struct nc_rpc *rpc;
  NC_WD_MODE wd = NC_WD_UNKNOWN;
  NC_PARAMTYPE param = NC_PARAMTYPE_CONST;
  char *start_time = NULL, *stop_time = NULL;

  /* create requests */
  rpc = nc_rpc_subscribe(stream, NULL, start_time, stop_time, param);
  AssertError(rpc != NULL, return EXIT_FAILURE, "<subscribe> RPC creation failed.\n");

  /* create notification thread so that notifications can immediately be received */
  int ret = 0;
#ifdef V1
  if (!nc_session_ntf_thread_running(ru_session->session)) {
    nc_session_set_data(ru_session->session, answer);
    ret = nc_recv_notif_dispatch(ru_session->session, notif_clb_v1);
    AssertError(ret == 0, return EXIT_FAILURE, "Failed to create notification thread.\n");
  }
#elif V2
  ret = nc_recv_notif_dispatch_data(ru_session->session, notif_clb_v2, answer, NULL);
  AssertError(ret == 0, return EXIT_FAILURE, "Failed to create notification thread.\n");
#endif

  ret = rpc_send_recv((struct nc_session *)ru_session->session, rpc, wd, timeout, NULL);
  AssertError(ret == 0, return EXIT_FAILURE, "Failed to subscribe to: stream = %s, filter = %s\n", stream, filter);

  free(start_time);
  free(stop_time);
  nc_rpc_free(rpc);

  return EXIT_SUCCESS;
}
