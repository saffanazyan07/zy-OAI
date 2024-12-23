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

#include "connect-mplane.h"
#include "common/utils/assertions.h"

#include <libyang/libyang.h>
#include <nc_client.h>

#define HOME_DIR (getenv("HOME"))

static int my_auth_hostkey_check(const char *hostname, ssh_session session, void *priv)
{
  (void)hostname;
  (void)session;
  (void)priv;

  return 0;
}

int connect_mplane(ru_session_t *ru_session)
{
  int port = NC_PORT_SSH;
  char *user = "oranbenetel";

  nc_client_ssh_set_username(user);

  nc_client_ssh_set_auth_pref(NC_SSH_AUTH_PASSWORD, -1);
  nc_client_ssh_set_auth_pref(NC_SSH_AUTH_PUBLICKEY, 1);  // ssh-key identification
  nc_client_ssh_set_auth_pref(NC_SSH_AUTH_INTERACTIVE, -1);

  char pub_key[64], priv_key[64];
  sprintf(pub_key, "%s%s", HOME_DIR, "/.ssh/id_rsa.pub");
  sprintf(priv_key, "%s%s", HOME_DIR, "/.ssh/id_rsa");
  printf("pub_key = %s, prv_key = %s\n", pub_key, priv_key);
  int keypair_ret = nc_client_ssh_add_keypair(pub_key, priv_key);
  AssertError(keypair_ret == 0, return EXIT_FAILURE, "Unable to authenticate RU with IP address %s\n", ru_session->ru_ip_add);
  nc_client_ssh_set_auth_hostkey_check_clb(my_auth_hostkey_check, "DATA");  // host-key identification

  /* create the session */
  ru_session->session = nc_connect_ssh(ru_session->ru_ip_add, port, NULL);
  AssertError(ru_session->session != NULL, return EXIT_FAILURE, "RU IP address %s unreachable.\n", ru_session->ru_ip_add);

  printf("Successfuly connected to RU with IP address %s\n", ru_session->ru_ip_add);

  return EXIT_SUCCESS;
}

void disconnect_mplane(void *rus_disconnect)
{
  ru_session_list_t *ru_session_list = (ru_session_list_t *)rus_disconnect;

  for (size_t i = 0; i <ru_session_list->num_rus; i++) {
    ru_session_t *ru_session = &ru_session_list->ru_session[i];
    if (ru_session->session == NULL)
      continue;
    printf("[MPLANE] Disconnecting from RU with IP %s\n.", ru_session->ru_ip_add);
    nc_session_free(ru_session->session, NULL);
    ru_session->session = NULL;
  }

  nc_client_destroy();
}
