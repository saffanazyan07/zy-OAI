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

#include <stdio.h>
#include <arpa/inet.h>
#include <time.h>

#include "common/utils/LOG/log.h"
#include "common/utils/ocp_itti/intertask_interface.h" // this pulls in all of OAI+4G+5G
#include "openair3/SCTP/sctp_eNB_task.h"
#include "openair2/E1AP/e1ap.h"
#include "openair3/ocp-gtpu/gtp_itf.h"

configmodule_interface_t *uniqCfg; /* TODO this should not be necessary? */

/* bullshit declarations pulled in by libs TODO remove */
int nr_rlc_get_available_tx_space(const rnti_t rntiP, const logical_chan_id_t channel_idP) { abort(); return 0; } /* in GTP */
softmodem_params_t *get_softmodem_params(void) { static softmodem_params_t p = {0}; return &p; }; /* in ITTI */
void e1_bearer_context_setup(const e1ap_bearer_setup_req_t *req) { abort(); } /* CU-UP */
void e1_bearer_context_modif(const e1ap_bearer_mod_req_t *req) { abort(); } /* CU-UP */
void e1_bearer_release_cmd(const e1ap_bearer_release_cmd_t *cmd) { abort(); } /* CU-UP */
instance_t *N3GTPUInst = NULL; /* CU-UP */
instance_t CUuniqInstance=0; /* CU-UP */

void exit_function(const char *file, const char *function, const int line, const char *s, const int assert)
{
  LOG_E(GNB_APP, "error at %s:%d:%s: %s\n", file, line, function, s);
  abort();
}

/* @brief Dummy function to pass a fptr to ITTI for staring RRC queue, without
 * actually handling any messages. Functions called by main() will send/receive
 * ITTI messages to E1, as if it was RRC. */
static void *cuup_tester_rrc(void *)
{
  itti_mark_task_ready(TASK_RRC_GNB);
  LOG_I(GNB_APP, "created RRC queue\n");
  return NULL;
}

/* @brief Initialize E1 interface (as if it was CU-CP) */
static void init_e1()
{
  net_ip_address_t ip = {.ipv4 = 1, .ipv4_address = "127.0.0.1" };
  e1ap_net_config_t conf = { .CUCP_e1_ip_address = ip };
  MessageDef *msg = itti_alloc_new_message(TASK_GNB_APP, 0, E1AP_REGISTER_REQ);
  e1ap_net_config_t *e1ap_nc = &E1AP_REGISTER_REQ(msg).net_config;
  *e1ap_nc = conf;
  itti_send_msg_to_task(TASK_CUCP_E1, 0, msg);
}

/* @brief Setup CU-UP connection and retrieve parameters for further
 * connection.
 *
 * Waits for E1 Setup Request, sends response, and returns SCTP association ID
 * and NSSAI used by this CU-UP.
 */
static void setup_cuup(sctp_assoc_t *assoc_id, e1ap_nssai_t *nssai)
{
  /* Wait for E1AP setup request, store CU-UP info */
  MessageDef *itti_req = NULL;
  itti_receive_msg(TASK_RRC_GNB, &itti_req);
  MessagesIds id = ITTI_MSG_ID(itti_req);
  AssertFatal(id == E1AP_SETUP_REQ, "expected E1AP setup request, received %s instead\n", itti_get_message_name(id));
  *assoc_id = itti_req->ittiMsgHeader.originInstance;
  e1ap_setup_req_t *req = &E1AP_SETUP_REQ(itti_req);
  long t_id = req->transac_id;
  DevAssert(req->supported_plmns = 1);
  DevAssert(req->plmn[0].slice != NULL);
  *nssai = *req->plmn[0].slice;
  DevAssert(assoc_id != 0);
  itti_free(TASK_GNB_APP, itti_req);

  // acknowledge the E1 setup request with a response
  MessageDef *itti_resp = itti_alloc_new_message(TASK_GNB_APP, 0, E1AP_SETUP_RESP);
  itti_resp->ittiMsgHeader.originInstance = *assoc_id;
  e1ap_setup_resp_t *resp = &E1AP_SETUP_RESP(itti_resp);
  resp->transac_id = t_id;
  itti_send_msg_to_task(TASK_CUCP_E1, 0, itti_resp);
}

/* @brief get E1AP bearer setup request based on parameters */
static e1ap_bearer_setup_req_t get_breq(uint32_t ue_id, long pdu_id, long drb_id, const e1ap_nssai_t *nssai, const UP_TL_information_t *tnl)
{
  e1ap_bearer_setup_req_t bearer_req = {
    .gNB_cu_cp_ue_id = ue_id,
    .cipheringAlgorithm = 0, /* no ciphering/NEA0 */
    .integrityProtectionAlgorithm = 0, /* no integrity/NIA0 */
    /* encryptionKey not needed */
    /* integrityProtectionKey not needed */
    .numPDUSessions = 1,
  };
  pdu_session_to_setup_t *pdu = &bearer_req.pduSession[0];
  pdu->sessionId = pdu_id;
  pdu->nssai = *nssai;
  pdu->securityIndication.integrityProtectionIndication = SECURITY_NOT_NEEDED;
  pdu->securityIndication.confidentialityProtectionIndication = SECURITY_NOT_NEEDED;
  pdu->UP_TL_information = *tnl;
  pdu->numDRB2Setup = 1;
  DRB_nGRAN_to_setup_t *drb = &pdu->DRBnGRanList[0];
  drb->id = drb_id;
  drb->numQosFlow2Setup = 1;
  drb->numCellGroups = 1;

  return bearer_req;
}

/* @brief Set up the NG-U (north-bound) GTP tunnel, and inform CU-UP via E1 */
static up_params_t setup_cuup_ue_ng(sctp_assoc_t assoc_id, const e1ap_nssai_t *nssai, uint32_t ue_id, instance_t gtp_inst, const char *ip, uint16_t port)
{
  long pdu_id = 1;
  long drb_id = pdu_id + 3;
  in_addr_t addr_lo;
  inet_pton(AF_INET, ip, &addr_lo);

  /* create the local tunnel (i.e., as if we were UPF) */
  transport_layer_addr_t null_addr = {.length = 32};
  teid_t teid_lo = newGtpuCreateTunnel(gtp_inst, ue_id, pdu_id, pdu_id, -1, -1, null_addr, port, NULL, NULL);
  UP_TL_information_t tnl = {.teId = teid_lo};
  memcpy(&tnl.tlAddress, &addr_lo, 4);

  /* Create a new bearer */
  MessageDef *itti_breq = itti_alloc_new_message(TASK_GNB_APP, 0, E1AP_BEARER_CONTEXT_SETUP_REQ);
  itti_breq->ittiMsgHeader.originInstance = assoc_id;
  e1ap_bearer_setup_req_t *breq = &E1AP_BEARER_CONTEXT_SETUP_REQ(itti_breq);
  *breq = get_breq(ue_id, pdu_id, drb_id, nssai, &tnl);
  itti_send_msg_to_task(TASK_CUCP_E1, 0, itti_breq);

  /* Wait for the ack */
  MessageDef *itti_bresp;
  itti_receive_msg(TASK_RRC_GNB, &itti_bresp);
  MessagesIds id = ITTI_MSG_ID(itti_bresp);
  AssertFatal(id == E1AP_BEARER_CONTEXT_SETUP_RESP, "expected E1AP bearer context setup response, received %s instead\n", itti_get_message_name(id));
  DevAssert(assoc_id == itti_bresp->ittiMsgHeader.originInstance);
  e1ap_bearer_setup_resp_t *bresp = &E1AP_BEARER_CONTEXT_SETUP_RESP(itti_bresp);
  DevAssert(bresp->gNB_cu_cp_ue_id == ue_id);
  DevAssert(bresp->gNB_cu_up_ue_id == ue_id);
  DevAssert(bresp->numPDUSessions == 1);
  teid_t teid_rm = bresp->pduSession[0].teId;
  in_addr_t addr_rm;
  memcpy(&addr_rm, &bresp->pduSession[0].tlAddress, 4);
  DevAssert(bresp->pduSession[0].numDRBSetup == 1);
  DRB_nGRAN_setup_t *drb = &bresp->pduSession[0].DRBnGRanList[0];
  DevAssert(drb->id == drb_id);
  DevAssert(drb->numUpParam == 1);
  up_params_t f1_up = drb->UpParamList[0];
  itti_free(TASK_GNB_APP, itti_bresp);

  /* print diagnostics */
  char ip_lo[32] = {0};
  inet_ntop(AF_INET, &addr_lo, ip_lo, sizeof(ip_lo));
  char ip_rm[32] = {0};
  inet_ntop(AF_INET, &addr_rm, ip_rm, sizeof(ip_rm));
  GtpuUpdateTunnelOutgoingAddressAndTeid(gtp_inst, ue_id, pdu_id, addr_rm, teid_rm);
  LOG_I(GNB_APP, "CU-UP created NG-U, local %s/%x remote %s/%x (port %d)\n", ip_lo, teid_lo, ip_rm, teid_rm, port);

  inet_ntop(AF_INET, &f1_up.tlAddress, ip_rm, sizeof(ip_rm));
  LOG_I(GNB_APP, "CU-UP created F1-U, remove %s/%lx\n", ip_rm, f1_up.teId);
  return f1_up;
}

/* @brief get E1AP bearer modification request based on parameters */
static e1ap_bearer_mod_req_t get_bmod(uint32_t ue_id, long pdu_id, up_params_t up)
{
  e1ap_bearer_mod_req_t bearer_mod = {
    .gNB_cu_cp_ue_id = ue_id,
    .gNB_cu_up_ue_id = ue_id,
    .numPDUSessionsMod = 1,
  };
  pdu_session_to_mod_t *pdum = &bearer_mod.pduSessionMod[0];
  pdum->sessionId = pdu_id;
  pdum->numDRB2Modify = 1;
  DRB_nGRAN_to_mod_t *drb = &pdum->DRBnGRanModList[0];
  drb->id = 4;
  drb->numDlUpParam = 1;
  drb->DlUpParamList[0] = up;

  return bearer_mod;
}

static uint32_t count = 0;
static uint64_t received = 0;
/* @brief Received callback for F1-U
 *
 * Checks for packet loss by verifying that the first 4 bytes are corresponding
 * to a continuous 4-byte (word) enumeration. */
static bool recv_f1(protocol_ctxt_t *ctxt,
                    const srb_flag_t flag,
                    const rb_id_t rb,
                    const mui_t mui,
                    const confirm_t confirm,
                    const sdu_size_t size,
                    unsigned char *const buf,
                    const pdcp_transmission_mode_t modeP,
                    const uint32_t *sourceL2Id,
                    const uint32_t *destinationL2Id)
{
  int skip_bytes = 2;
  uint32_t *payload = (uint32_t *) &buf[skip_bytes];
  //LOG_I(GNB_APP, "received %d bytes for UE %ld (c %d)n", size, ctxt->rntiMaybeUEid, c);
  // check that first and last byte correspond to current count
  DevAssert((size - skip_bytes) % 4 == 0);
  int n = (size - skip_bytes) / 4;
  if (payload[0] != count) {
    uint32_t diff = payload[0] - count;
    LOG_W(GNB_APP, "packet loss: expected %d received %d diff %d (%d packets)\n", count, payload[0], diff, diff / 1400);
  }
  count = payload[n - 1] + 1;
  received += size - skip_bytes;

  return true;
}

/* @brief Set up the F1-U (south-bound) GTP tunnel, and inform CU-UP via E1 */
static void setup_cuup_ue_f1(sctp_assoc_t assoc_id, uint32_t ue_id, instance_t gtp_inst, const char *ip, uint16_t port, up_params_t rm)
{
  int pdu_id = 1;
  int drb_id = pdu_id + 3;

  /* create tunnel (i.e., as if we were DU) */
  transport_layer_addr_t addr = {.length = 32};
  memcpy(addr.buffer, &rm.tlAddress, 4);
  teid_t teid_lo = newGtpuCreateTunnel(gtp_inst, ue_id, drb_id, drb_id, rm.teId, -1, addr, port, recv_f1, NULL);

  up_params_t lo = {.teId = teid_lo, .cell_group_id = rm.cell_group_id};
  inet_pton(AF_INET, ip, &lo.tlAddress);

  /* modify existing tunnel via E1 to pass in F1-U TEID */
  MessageDef *itti_bmod = itti_alloc_new_message(TASK_GNB_APP, 0, E1AP_BEARER_CONTEXT_MODIFICATION_REQ);
  itti_bmod->ittiMsgHeader.originInstance = assoc_id;
  e1ap_bearer_mod_req_t *bmod = &E1AP_BEARER_CONTEXT_MODIFICATION_REQ(itti_bmod);
  *bmod = get_bmod(ue_id, pdu_id, lo);
  itti_send_msg_to_task(TASK_CUCP_E1, 0, itti_bmod);

  /* Wait for the ack */
  MessageDef *itti_bmodr;
  itti_receive_msg(TASK_RRC_GNB, &itti_bmodr);
  MessagesIds id = ITTI_MSG_ID(itti_bmodr);
  AssertFatal(id == E1AP_BEARER_CONTEXT_MODIFICATION_RESP, "expected E1AP bearer context modification response, received %s instead\n", itti_get_message_name(id));
  //DevAssert(assoc_id == itti_bmodr->ittiMsgHeader.originInstance);
  itti_free(TASK_GNB_APP, itti_bmodr);

  /* print diagnostics */
  char ip_lo[32] = {0};
  inet_ntop(AF_INET, &lo.tlAddress, ip_lo, sizeof(ip_lo));
  char ip_rm[32] = {0};
  inet_ntop(AF_INET, &rm.tlAddress, ip_rm, sizeof(ip_rm));
  LOG_I(GNB_APP, "CU-UP created F1-U, local %s/%lx remote %s/%lx (port %d)\n", ip_lo, lo.teId, ip_rm, rm.teId, port);
}

/* @brief initialize GTP interface for IP:port */
static instance_t init_gtp(const char *ip, uint16_t port)
{
  openAddr_t iface = {0};
  DevAssert(strlen(ip) <= sizeof(iface.originHost));
  strcpy(iface.originHost, ip);
  snprintf(iface.originService, sizeof(iface.originService), "%d", port);
  return gtpv1Init(iface);
}

/* @brief calculate time difference of two struct timespec */
static float time_diff(struct timespec s, struct timespec e)
{
  //printf("start %ld.%ld end %ld.%ld\n", s.tv_sec, s.tv_nsec, e.tv_sec, e.tv_nsec);
  e.tv_sec -= s.tv_sec;
  e.tv_nsec = (e.tv_nsec + 1000000000 - s.tv_nsec) % 1000000000;
  float dur = (float) e.tv_sec + e.tv_nsec / 1000000000.f;
  //printf("duration %ld.%ld duration %f s\n", end.tv_sec, end.tv_nsec, dur);
  return dur;
}

int main(int argc, char *argv[])
{
  /* TODO: do without: why is this necessary? */
  if ((uniqCfg = load_configmodule(argc, argv, CONFIG_ENABLECMDLINEONLY)) == NULL) {
    exit_fun("[SOFTMODEM] Error, configuration module init failed\n");
  }
  logInit();
  itti_init(TASK_MAX, tasks_info);

  const char *ng_ip = "127.0.0.1";
  uint16_t ng_port = 2152;
  instance_t ng_inst = init_gtp(ng_ip, ng_port);

  const char *f1_ip = "127.0.0.1";
  uint16_t f1_port = 2153;
  instance_t f1_inst = init_gtp(f1_ip, f1_port);

  int rc;
  rc = itti_create_task(TASK_SCTP, sctp_eNB_task, NULL);
  AssertFatal(rc >= 0, "Create task for SCTP failed\n");
  rc = itti_create_task(TASK_CUCP_E1, E1AP_CUCP_task, NULL);
  AssertFatal(rc >= 0, "Create task for CUUP E1 failed\n");
  rc = itti_create_task(TASK_GTPV1_U, gtpv1uTask, NULL);
  AssertFatal(rc >= 0, "Create task for GTPV1U failed\n");

  rc = itti_create_task(TASK_RRC_GNB, cuup_tester_rrc, NULL);
  AssertFatal(rc >= 0, "RRC");
  init_e1();

  sctp_assoc_t assoc_id;
  e1ap_nssai_t nssai;
  setup_cuup(&assoc_id, &nssai);

  uint32_t ue_id = 1;
  up_params_t f1_up = setup_cuup_ue_ng(assoc_id, &nssai, ue_id, ng_inst, ng_ip, ng_port);
  setup_cuup_ue_f1(assoc_id, ue_id, f1_inst, f1_ip, f1_port, f1_up);

  sleep(1);

  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  uint32_t i = 0;
  uint64_t total = 0;
  while (i < 500000000) {
    char buf[1400];
    size_t len = sizeof(buf);
    DevAssert(len == 1400);
    uint32_t *p = (uint32_t *)buf;
    for (int j = 0; j < 350; ++j)
      *p++ = i++;
    gtpv1uSendDirect(ng_inst, ue_id, 1, (uint8_t *)buf, len, false, false);
    total += len;
    if ((i / 1400) % 1000 == 0) {
      printf("%d\r", i);
      fflush(stdout);
    }
    usleep(2);
  }
  struct timespec end;
  clock_gettime(CLOCK_MONOTONIC, &end);

  printf("%d => finished\n", i);
  sleep(1);
  printf("sent %ld received %ld bytes\n", total, received);

  float dur = time_diff(t, end);
  float thr = total * 8 / dur / 1000000.f;
  printf("sent %ld bytes in %.3f s => %.3f Mbps\n", total, dur, thr);

  sleep(1);
  LOG_I(GNB_APP, "bye\n");
}
