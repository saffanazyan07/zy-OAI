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

#include "config-mplane.h"
#include "rpc-send-recv.h"
#include "common/utils/assertions.h"

#include <libyang/libyang.h>
#include <nc_client.h>

int edit_config_mplane(ru_session_t *ru_session)
{
  int timeout = CLI_RPC_REPLY_TIMEOUT;
  struct nc_rpc *rpc;
  NC_WD_MODE wd = NC_WD_UNKNOWN;
  NC_PARAMTYPE param = NC_PARAMTYPE_CONST;
  NC_DATASTORE target = NC_DATASTORE_CANDIDATE;
  NC_RPC_EDIT_DFLTOP op = NC_RPC_EDIT_DFLTOP_MERGE;
  NC_RPC_EDIT_TESTOPT test = NC_RPC_EDIT_TESTOPT_UNKNOWN;
  NC_RPC_EDIT_ERROPT err = NC_RPC_EDIT_ERROPT_UNKNOWN;

  // the following block is just temporary; the buffer should be passed, not reading from a file
  const char *input = "/home/eurecom/teodora/xml-mplane/550-1e-config-backup.xml";
  FILE *f = fopen(input, "r");
  fseek(f, 0, SEEK_END);
  long len = ftell(f);
  fseek(f, 0, SEEK_SET);
  char *content = calloc(len + 1, sizeof(char));
  int ret2 = fread(content, 1, len, f);
  AssertError(ret2 == 0, return EXIT_FAILURE, "Just temp");
  content[len] = '\0';
  fclose(f);

  rpc = nc_rpc_edit(target, op, test, err, content, param);
  AssertError(rpc != NULL, return EXIT_FAILURE, "<edit-config> RPC creation failed.\n");

  int ret = rpc_send_recv((struct nc_session *)ru_session->session, rpc, wd, timeout, NULL);
  AssertError(ret == 0, return EXIT_FAILURE, "Failed to edit configuration for the candidate datastore.\n");

  nc_rpc_free(rpc);
  free(content);

  return EXIT_SUCCESS;
}

int validate_config_mplane(ru_session_t *ru_session)
{
  int timeout = CLI_RPC_REPLY_TIMEOUT;
  struct nc_rpc *rpc;
  NC_WD_MODE wd = NC_WD_UNKNOWN;
  NC_PARAMTYPE param = NC_PARAMTYPE_CONST;
  char *src_start = NULL;
  NC_DATASTORE source = NC_DATASTORE_CANDIDATE;

  rpc = nc_rpc_validate(source, src_start, param);
  AssertError(rpc != NULL, return EXIT_FAILURE, "<validate> RPC creation failed.\n");

  int ret = rpc_send_recv((struct nc_session *)ru_session->session, rpc, wd, timeout, NULL);
  AssertError(ret == 0, return EXIT_FAILURE, "Failed to validate candidate datastore.\n");

  nc_rpc_free(rpc);

  return EXIT_SUCCESS;
}

int commit_config_mplane(ru_session_t *ru_session)
{
  int timeout = CLI_RPC_REPLY_TIMEOUT;
  struct nc_rpc *rpc;
  NC_WD_MODE wd = NC_WD_UNKNOWN;
  NC_PARAMTYPE param = NC_PARAMTYPE_CONST;
  int confirmed = 0;
  int32_t confirm_timeout = 0;
  char *persist = NULL, *persist_id = NULL;

  rpc = nc_rpc_commit(confirmed, confirm_timeout, persist, persist_id, param);
  AssertError(rpc != NULL, return EXIT_FAILURE, "<commit> RPC creation failed.\n");

  int ret = rpc_send_recv((struct nc_session *)ru_session->session, rpc, wd, timeout, NULL);
  AssertError(ret == 0, return EXIT_FAILURE, "Failed to commit candidate datastore.\n");

  nc_rpc_free(rpc);

  return EXIT_SUCCESS;
}
