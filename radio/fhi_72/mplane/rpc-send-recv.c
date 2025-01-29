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

#include "rpc-send-recv.h"

#include "common/utils/assertions.h"


#ifdef MPLANE_V1
static int recv_v1(struct nc_session *session, struct nc_rpc *rpc, NC_MSG_TYPE msgtype, const uint64_t msgid, int timeout_s, char **answer)
{
  struct nc_reply *reply;
  struct nc_reply_data *data_rpl;
  struct nc_reply_error *error;
  char *str = NULL;
  uint32_t ly_wd;

  LYD_FORMAT output_format = LYD_XML;

  uint32_t output_flag = 0;    // other option is LYD_PRINT_SHRINK: Flag for output without indentation and formatting new lines.

  while(1){
    msgtype = nc_recv_reply(session, rpc, msgid, timeout_s * 10000,
                            LYD_OPT_DESTRUCT | LYD_OPT_NOSIBLINGS, &reply);
    if (msgtype == NC_MSG_ERROR) {
      AssertError(false, return EXIT_FAILURE, "[MPLANE] Failed to receive a reply.");
    } else if (msgtype == NC_MSG_WOULDBLOCK) {
      AssertError(false, return EXIT_FAILURE, "[MPLANE] Timeout for receiving a reply expired.");
    } else if (msgtype == NC_MSG_NOTIF) {
      /* read again */
      continue;
    } else if (msgtype == NC_MSG_REPLY_ERR_MSGID) {
      /* unexpected message, try reading again to get the correct reply */
      LOG_I(HW, "[MPLANE] Unexpected reply received - ignoring and waiting for the correct reply.\n");
      nc_reply_free(reply);
      continue;
    }
    break;
  }

  switch (reply->type) {
    case NC_RPL_OK:
      LOG_I(HW, "[MPLANE] RPC reply = OK\n");
      break;
    case NC_RPL_DATA:
      data_rpl = (struct nc_reply_data *)reply;

      if (nc_rpc_get_type(rpc) == NC_RPC_GETSCHEMA) {
        AssertError((!data_rpl->data || (data_rpl->data->schema->nodetype != LYS_RPC) || (data_rpl->data->child == NULL)
               || (data_rpl->data->child->schema->nodetype != LYS_ANYXML)), return EXIT_FAILURE, "[MPLANE] Cannot get schema");

        struct lyd_node_anydata *any = (struct lyd_node_anydata *)data_rpl->data->child;
        switch (any->value_type) {
          case LYD_ANYDATA_CONSTSTRING:
          case LYD_ANYDATA_STRING:
            *answer = strdup(any->value.str); 
            break;
          case LYD_ANYDATA_DATATREE:
            lyd_print_mem(answer, any->value.tree, LYD_XML, LYP_FORMAT | LYP_WITHSIBLINGS);
            break;
          case LYD_ANYDATA_XML:
            lyxml_print_mem(answer, any->value.xml, LYXML_PRINT_SIBLINGS);
            break;
          default:
            AssertError(false, return EXIT_FAILURE, "[MPLANE] Unknown yang type.\n");
          }
          break;
      } else if (nc_rpc_get_type(rpc) == NC_RPC_GETCONFIG) {
        char *buffer = NULL;
        ly_wd = 0; // but try with EXPLICIT
        lyd_print_mem(&buffer, data_rpl->data, output_format, LYP_WITHSIBLINGS | LYP_NETCONF | ly_wd | output_flag);
        *answer = calloc(strlen(buffer)+128, sizeof(char));
        sprintf(*answer, "%s%s%s", "<config xmlns=\"urn:ietf:params:xml:ns:netconf:base:1.0\">\n", buffer, "</config>");
      } else if (nc_rpc_get_type(rpc) == NC_RPC_GET) {
        char *buffer = NULL;
        ly_wd = 0; // but try with EXPLICIT
        lyd_print_mem(&buffer, data_rpl->data, output_format, LYP_WITHSIBLINGS | LYP_NETCONF | ly_wd | output_flag);
        *answer = calloc(strlen(buffer)+128, sizeof(char));
        sprintf(*answer, "%s%s%s", "<data xmlns=\"urn:ietf:params:xml:ns:netconf:base:1.0\">\n", buffer, "</data>");
      }
      
      break;
    case NC_RPL_ERROR:
      LOG_I(HW, "[MPLANE] ERROR\n");
      error = (struct nc_reply_error *)reply;
      for (int i = 0; i < error->count; ++i) {
        if (error->err[i].type) {
          printf("\ttype:     %s\n", error->err[i].type);
        }
        if (error->err[i].tag) {
          printf("\ttag:      %s\n", error->err[i].tag);
        }
        if (error->err[i].severity) {
          printf("\tseverity: %s\n", error->err[i].severity);
        }
        if (error->err[i].apptag) {
          printf("\tapp-tag:  %s\n", error->err[i].apptag);
        }
        if (error->err[i].path) {
          printf("\tpath:     %s\n", error->err[i].path);
        }
        if (error->err[i].message) {
          printf("\tmessage:  %s\n", error->err[i].message);
        }
        if (error->err[i].sid) {
          printf("\tSID:      %s\n", error->err[i].sid);
        }
        for (int j = 0; j < error->err[i].attr_count; ++j) {
          printf("\tbad-attr #%d: %s\n", j + 1, error->err[i].attr[j]);
        }
        for (int j = 0; j < error->err[i].elem_count; ++j) {
          printf("\tbad-elem #%d: %s\n", j + 1, error->err[i].elem[j]);
        }
        for (int j = 0; j < error->err[i].ns_count; ++j) {
          printf("\tbad-ns #%d:   %s\n", j + 1, error->err[i].ns[j]);
        }
        for (int j = 0; j < error->err[i].other_count; ++j) {
          lyxml_print_mem(&str, error->err[i].other[j], 0);
          printf("\tother #%d:\n%s\n", j + 1, str);
          free(str);
        }
        printf("\n");
      }
      AssertError(false, return EXIT_FAILURE, "[MPLANE] Unable to continue.\n");
      break;
    default:
      AssertError(false, return EXIT_FAILURE, "[MPLANE] Internal error.\n");
      nc_reply_free(reply);
  }
  nc_reply_free(reply);

  return EXIT_SUCCESS;
}
#endif

#ifdef MPLANE_V2
static int recv_v2(struct nc_session *session, struct nc_rpc *rpc, NC_MSG_TYPE msgtype, const uint64_t msgid, int timeout_s, char **answer)
{
  struct lyd_node *envp, *op, *err, *node, *info;
  uint32_t ly_wd;
  LYD_FORMAT output_format = LYD_XML;
  uint32_t output_flag = 0;    // other option is LYD_PRINT_SHRINK: Flag for output without indentation and formatting new lines.

  while(1){
    msgtype = nc_recv_reply(session, rpc, msgid, timeout_s * 10000, &envp, &op);
    if (msgtype == NC_MSG_ERROR) {
      AssertError(false, return EXIT_FAILURE, "[MPLANE] Failed to receive a reply.");
    } else if (msgtype == NC_MSG_WOULDBLOCK) {
      AssertError(false, return EXIT_FAILURE, "[MPLANE] Timeout for receiving a reply for RPC expired.");
    } else if (msgtype == NC_MSG_NOTIF) {    // for SUBSCRIBE 
      /* read again */
      continue;
    } else if (msgtype == NC_MSG_REPLY_ERR_MSGID) {
      /* unexpected message, try reading again to get the correct reply */
      LOG_I(HW, "[MPLANE] Unexpected reply received - ignoring and waiting for the correct reply.\n");
      lyd_free_tree(envp);
      lyd_free_tree(op);
      continue;
    }
    break;
  }

  /* get functionality */
  if (op) {
  /* data reply */
    if (nc_rpc_get_type(rpc) == NC_RPC_GETSCHEMA) {
      /* special case */
      if (!lyd_child(op) || (lyd_child(op)->schema->nodetype != LYS_ANYXML)) {
        AssertError(false, return EXIT_FAILURE, "[MPLANE] Cannot get schema.");
      }
      struct lyd_node_any *any = (struct lyd_node_any *)lyd_child(op);
      switch (any->value_type) {
      case LYD_ANYDATA_STRING:
      case LYD_ANYDATA_XML:
        *answer = strdup(any->value.str);
        break;
      case LYD_ANYDATA_DATATREE:
        lyd_print_mem(answer, any->value.tree, LYD_XML, LYD_PRINT_WITHSIBLINGS);
        break;
      default:
        AssertError(false, return EXIT_FAILURE, "[MPLANE] Unexpected yang type.");
      }
    } else {

      ly_wd = 0;  // but try with EXPLICIT
      // switch (wd_mode) {
      // case NC_WD_ALL:
      //     ly_wd = LYD_PRINT_WD_ALL;
      //     break;
      // case NC_WD_ALL_TAG:
      //     ly_wd = LYD_PRINT_WD_ALL_TAG;
      //     break;
      // case NC_WD_TRIM:
      //     ly_wd = LYD_PRINT_WD_TRIM;
      //     break;
      // case NC_WD_EXPLICIT:
      //     ly_wd = LYD_PRINT_WD_EXPLICIT;
      //     break;
      // default:
      //     ly_wd = 0;
      //     break;
      // }

      // lyd_print_file(output, lyd_child(op), output_format, LYD_PRINT_WITHSIBLINGS | ly_wd | output_flag);
      lyd_print_mem(answer, lyd_child(op), output_format, LYD_PRINT_WITHSIBLINGS | ly_wd | output_flag);
    }
  /* edit/validate/commit functionalities */
  } else if (!strcmp(LYD_NAME(lyd_child(envp)), "ok")) {
    LOG_I(HW, "[MPLANE] RPC reply = OK\n");
  } else {
    //assert(!strcmp(LYD_NAME(lyd_child(envp)), "rpc-error")); check why

    /* make sure the following code is correct, and try to make it shorter? */
    LOG_I(HW, "[MPLANE] ERROR\n");
    LY_LIST_FOR(lyd_child(envp), err) {
      lyd_find_sibling_opaq_next(lyd_child(err), "error-type", &node);
      if (node) {
        printf("\ttype:     %s\n", ((struct lyd_node_opaq *)node)->value);
      }
      lyd_find_sibling_opaq_next(lyd_child(err), "error-tag", &node);
      if (node) {
        printf("\ttag:      %s\n", ((struct lyd_node_opaq *)node)->value);
      }
      lyd_find_sibling_opaq_next(lyd_child(err), "error-severity", &node);
      if (node) {
        printf("\tseverity: %s\n", ((struct lyd_node_opaq *)node)->value);
      }
      lyd_find_sibling_opaq_next(lyd_child(err), "error-app-tag", &node);
      if (node) {
        printf("\tapp-tag:  %s\n", ((struct lyd_node_opaq *)node)->value);
      }
      lyd_find_sibling_opaq_next(lyd_child(err), "error-path", &node);
      if (node) {
        printf("\tpath:     %s\n", ((struct lyd_node_opaq *)node)->value);
      }
      lyd_find_sibling_opaq_next(lyd_child(err), "error-message", &node);
      if (node) {
        printf("\tmessage:  %s\n", ((struct lyd_node_opaq *)node)->value);
      }

      info = lyd_child(err);
      while (!lyd_find_sibling_opaq_next(info, "error-info", &info)) {
        printf("\tinfo:\n");
        lyd_print_file(stdout, lyd_child(info), output_format, LYD_PRINT_WITHSIBLINGS);

        info = info->next;
      }
      printf("\n");
    }
    AssertError(false, return EXIT_FAILURE, "[MPLANE] Unable to continue.\n");
  }

  lyd_free_tree(envp);
  lyd_free_tree(op);

  return EXIT_SUCCESS;
}
#endif

int rpc_send_recv(struct nc_session *session, struct nc_rpc *rpc, NC_WD_MODE wd_mode, int timeout_s, char **answer)
{
  uint64_t msgid;
  NC_MSG_TYPE msgtype;

  msgtype = nc_send_rpc(session, rpc, 1000, &msgid);
  if (msgtype == NC_MSG_ERROR) {
    AssertError(false, return EXIT_FAILURE, "[MPLANE] Failed to send the RPC.\n");
  } else if (msgtype == NC_MSG_WOULDBLOCK) {
    AssertError(false, return EXIT_FAILURE, "[MPLANE] Timeout for sending the RPC expired.\n");
  }

#ifdef MPLANE_V1
  return recv_v1(session, rpc, msgtype, msgid, timeout_s, answer);
#elif defined MPLANE_V2
  return recv_v2(session, rpc, msgtype, msgid, timeout_s, answer);
#else
  AssertError(false, return EXIT_FAILURE, "[MPLANE] Unknown M-plane version found. Tried MPLANE_V1 and MPLANE_V2.\n");
#endif
}
