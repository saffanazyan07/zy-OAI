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

#define _GNU_SOURCE
#include <dlfcn.h>

#include "time_manager.h"

#include "time_source.h"
#include "time_server.h"
#include "time_client.h"

#include "common/config/config_userapi.h"
#include "common/utils/assertions.h"
#include "common/utils/LOG/log.h"

/* global variables (some may be ununsed, depends on configuration) */
static time_source_t *time_source = NULL;
static time_server_t *time_server = NULL;
static time_client_t *time_client = NULL;

/* available tick functions in the executable
 * found at startup time of the program using dlsym()
 */
static void (*pdcp_tick)(void) = NULL;
static void (*rlc_tick)(void) = NULL;
static void (*x2ap_tick)(void) = NULL;

/* tick callback, called by time_source every 'millisecond' (can be
 * actual millisecond or simulated millisecond)
 */
static void tick(void *unused)
{
  /* call x2ap_tick, pdcp_tick, rlc_tick (if they exist) */
  if (pdcp_tick != NULL)
    pdcp_tick();

  if (rlc_tick != NULL)
    rlc_tick();

  if (x2ap_tick != NULL)
    x2ap_tick();
}

void time_manager_start(time_manager_client_t client_type,
                        time_manager_mode_t running_mode)
{
  time_source_type_t time_source_type = -1;
  bool has_time_server = false;
  bool has_time_client = false;
  char *default_server_ip = "127.0.0.1";
  int default_server_port = 7374;
  char *server_ip = NULL;
  int server_port = -1;

  /* retrieve, if applicable, available tick functions in the program */
  if (client_type == TIME_MANAGER_GNB_MONOLITHIC
      || client_type == TIME_MANAGER_GNB_CU
      || client_type == TIME_MANAGER_UE)
    pdcp_tick = dlsym(RTLD_DEFAULT, "nr_pdcp_ms_tick");

  if (client_type == TIME_MANAGER_GNB_MONOLITHIC
      || client_type == TIME_MANAGER_GNB_DU
      || client_type == TIME_MANAGER_UE)
    rlc_tick = dlsym(RTLD_DEFAULT, "nr_rlc_ms_tick");

  /* let's also retrieve is_x2ap_enabled() with dlsym to not force the
   * application to define it (this logic may be changed if wanted)
   */
  int (*is_x2ap_enabled)(void) = dlsym(RTLD_DEFAULT, "is_x2ap_enabled");
  if ((client_type == TIME_MANAGER_GNB_MONOLITHIC
       || client_type == TIME_MANAGER_GNB_CU)
      && is_x2ap_enabled != NULL && is_x2ap_enabled())
    x2ap_tick = dlsym(RTLD_DEFAULT, "x2ap_ms_tick");

  /* get default configuration */
  switch (client_type) {
    case TIME_MANAGER_GNB_MONOLITHIC:
    case TIME_MANAGER_GNB_DU:
    case TIME_MANAGER_UE:
      switch (running_mode) {
        case TIME_MANAGER_REALTIME:
          time_source_type = TIME_SOURCE_REALTIME;
          break;
        case TIME_MANAGER_IQ_SAMPLES:
          time_source_type = TIME_SOURCE_IQ_SAMPLES;
          break;
      }
      break;
    case TIME_MANAGER_GNB_CU:
    case TIME_MANAGER_SIMULATOR:
      time_source_type = TIME_SOURCE_REALTIME;
      break;
  }

  /* get values from configuration */
  paramdef_t config_parameters[] = {
      {"time_source", "time source", 0, .strptr = NULL, .defstrval = NULL, TYPE_STRING, 0},
      {"mode", "time mode", 0, .strptr = NULL, .defstrval = NULL, TYPE_STRING, 0},
      {"server_ip", "server ip", 0, .strptr = NULL, .defstrval = NULL, TYPE_STRING, 0},
      {"server_port", "server port", 0, .iptr = NULL, .defintval = -1, TYPE_INT, 0},
  };
  int ret = config_get(config_get_if(), config_parameters, sizeofArray(config_parameters), "time_management");
  if (ret >= 0) {
#define TIME_SOURCE_IDX 0
#define MODE_IDX        1
#define SERVER_IP_IDX   2
#define SERVER_PORT_IDX 3
    char *param;
    /* time source */
    if (config_parameters[TIME_SOURCE_IDX].strptr != NULL)
      param = *config_parameters[TIME_SOURCE_IDX].strptr;
    else
      param = NULL;
    if (param != NULL) {
      if (!strcmp(param, "realtime")) {
        time_source_type = TIME_SOURCE_REALTIME;
      } else if (!strcmp(param, "iq_samples")) {
        time_source_type = TIME_SOURCE_IQ_SAMPLES;
      } else {
        AssertFatal(0, "bad parameter 'time_source' in section 'time_management', unknown value \"%s\". Valid values are \"realtime\" and \"iq_samples\"\n", param);
      }
    }

    /* mode */
    if (config_parameters[MODE_IDX].strptr != NULL)
      param = *config_parameters[MODE_IDX].strptr;
    else
      param = NULL;
    if (param != NULL) {
      if (!strcmp(param, "standalone")) {
        has_time_server = false;
        has_time_client = false;
      } else if (!strcmp(param, "server")) {
        has_time_server = true;
        has_time_client = false;
      } else if (!strcmp(param, "client")) {
        has_time_server = false;
        has_time_client = true;
      } else {
        AssertFatal(0, "bad parameter 'mode' in section 'time_management', unknown value \"%s\". Valid values are \"standalone\", \"server\" and \"client\"\n", param);
      }
    }

    /* server ip */
    if (config_parameters[SERVER_IP_IDX].strptr != NULL)
      param = *config_parameters[SERVER_IP_IDX].strptr;
    else
      param = NULL;
    if (param != NULL) {
      server_ip = param;
    }

    /* server port */
    int port = *config_parameters[SERVER_PORT_IDX].iptr;
    if (port != -1) {
      server_port = port;
    }
  }

  if (server_ip == NULL)
    server_ip = default_server_ip;

  if (server_port == -1)
    server_port = default_server_port;

  /* create entities, according to selected configuration */
  if (!has_time_client) {
    time_source = new_time_source(time_source_type);
    time_source_set_callback(time_source, tick, NULL);
  }

  if (has_time_server) {
    time_server = new_time_server(server_ip, server_port, tick, NULL);
    time_server_attach_time_source(time_server, time_source);
  }

  if (has_time_client) {
    time_client = new_time_client(server_ip, server_port, tick, NULL);
  }

  LOG_I(UTIL,
        "time manager configuration: [time source: %s] [mode: %s] [server IP: %s} [server port: %d]%s\n",
        (char *[]){"reatime", "iq_samples"}[time_source_type],
        has_time_server   ? "server"
        : has_time_client ? "client"
                          : "standalone",
        server_ip,
        server_port,
        has_time_server || has_time_client ? "" : " (server IP/port not used)");

  LOG_I(UTIL,
        "time manager: pdcp tick: %s, rlc tick: %s, x2ap tick: %s\n",
        pdcp_tick == NULL ? "not activated" : "activated",
        rlc_tick == NULL ? "not activated" : "activated",
        x2ap_tick == NULL ? "not activated" : "activated");
}

void time_manager_iq_samples(uint64_t iq_samples_count,
                             uint64_t iq_samples_per_second)
{
  time_source_iq_add(time_source, iq_samples_count, iq_samples_per_second);
}

void time_manager_finish(void)
{
  /* time source has to be shutdown first */
  if (time_source != NULL) {
    free_time_source(time_source);
    time_source = NULL;
  }

  if (time_server != NULL) {
    free_time_server(time_server);
    time_server = NULL;
  }

  if (time_client != NULL) {
    free_time_client(time_client);
    time_client = NULL;
  }
}
