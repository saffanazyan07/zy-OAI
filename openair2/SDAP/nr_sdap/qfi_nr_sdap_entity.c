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

#include "nr_sdap_entity.h"
#include <openair2/LAYER2/nr_pdcp/nr_pdcp_oai_api.h>
#include <openair3/ocp-gtpu/gtp_itf.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "PHY/defs_common.h"
#include "T.h"
#include "assertions.h"
#include "common/utils/T/T.h"
#include "gtpv1_u_messages_types.h"
#include "intertask_interface.h"
#include "rlc.h"
//zy
#include <linux/if_tun.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <net/if.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/if.h>
#include <arpa/inet.h>
#include <net/if_arp.h>
//#include <netinet/if_ether.h>
#include <linux/if_ether.h>

typedef struct {
    struct arphdr ea_hdr;
    uint8_t arp_sha[ETH_ALEN];
    uint8_t arp_spa[4];
    uint8_t arp_tha[ETH_ALEN];
    uint8_t arp_tpa[4];
} ether_arp_fixed_t;

//zy-e

typedef struct {
  nr_sdap_entity_t *sdap_entity_llist;
} nr_sdap_entity_info;

static nr_sdap_entity_info sdap_info;

instance_t *N3GTPUInst = NULL;

/**
 * @brief indicates whether it is a receiving SDAP entity
 *        i.e. for UE, header for DL data is present
 *             for gNB, header for UL data is present
 */
bool is_sdap_rx(bool is_gnb, NR_SDAP_Config_t *sdap_config)
{
  if (is_gnb) {
    return sdap_config->sdap_HeaderUL == NR_SDAP_Config__sdap_HeaderUL_present;
  } else {
    return sdap_config->sdap_HeaderDL == NR_SDAP_Config__sdap_HeaderDL_present;
  }
}

/**
 * @brief indicates whether it is a transmitting SDAP entity
 *        i.e. for UE, header for UL data is present
 *             for gNB, header for DL data is present
 */
bool is_sdap_tx(bool is_gnb, NR_SDAP_Config_t *sdap_config)
{
  if (is_gnb) {
    return sdap_config->sdap_HeaderDL == NR_SDAP_Config__sdap_HeaderDL_present;
  } else {
    return sdap_config->sdap_HeaderUL == NR_SDAP_Config__sdap_HeaderUL_present;
  }
}

/* ori
void nr_pdcp_submit_sdap_ctrl_pdu(ue_id_t ue_id, rb_id_t sdap_ctrl_pdu_drb, nr_sdap_ul_hdr_t ctrl_pdu)
{

  protocol_ctxt_t ctxt = { .rntiMaybeUEid = ue_id };
  nr_pdcp_data_req_drb(&ctxt,
                       SRB_FLAG_NO,
                       sdap_ctrl_pdu_drb,
                       RLC_MUI_UNDEFINED,
                       SDU_CONFIRM_NO,
                       SDAP_HDR_LENGTH,
                       (unsigned char *)&ctrl_pdu,
                       PDCP_TRANSMISSION_MODE_UNKNOWN,
                       NULL,
                       NULL);
  LOG_D(SDAP, "Control PDU - Submitting Control PDU to DRB ID:  %ld\n", sdap_ctrl_pdu_drb);
  LOG_D(SDAP, "QFI: %u\n R: %u\n D/C: %u\n", ctrl_pdu.QFI, ctrl_pdu.R, ctrl_pdu.DC);
  return;
}
*/
void nr_pdcp_submit_sdap_ctrl_pdu(ue_id_t ue_id, rb_id_t sdap_ctrl_pdu_drb, nr_sdap_ul_hdr_t ctrl_pdu)
{
  protocol_ctxt_t ctxt = { .rntiMaybeUEid = ue_id };

  /* Pack SDAP UL header in 1 byte: D/C (bit7), R (bit6), QFI (bits5..0) */
  uint8_t hdr = ((ctrl_pdu.DC & 0x1) << 7) |
                ((ctrl_pdu.R  & 0x1) << 6) |
                (ctrl_pdu.QFI & 0x3F);

  nr_pdcp_data_req_drb(&ctxt,
                      SRB_FLAG_NO,
                      sdap_ctrl_pdu_drb,
                      RLC_MUI_UNDEFINED,
                      SDU_CONFIRM_NO,
                      1, /* header length */
                      &hdr,
                      PDCP_TRANSMISSION_MODE_UNKNOWN,
                      NULL,
                      NULL);
  LOG_D(SDAP, "Control PDU - Submitted on DRB %ld (hdr=0x%02x) QFI:%u R:%u D/C:%u\n",
        sdap_ctrl_pdu_drb, hdr, ctrl_pdu.QFI, ctrl_pdu.R, ctrl_pdu.DC);
}

static bool nr_sdap_tx_entity(nr_sdap_entity_t *entity,
                              protocol_ctxt_t *ctxt_p,
                              const srb_flag_t srb_flag,
                              const rb_id_t rb_id,
                              const mui_t mui,
                              const confirm_t confirm,
                              const sdu_size_t sdu_buffer_size,
                              unsigned char *const sdu_buffer,
                              const pdcp_transmission_mode_t pt_mode,
                              const uint32_t *sourceL2Id,
                              const uint32_t *destinationL2Id,
                              const uint8_t qfi,
                              const bool rqi)
{
  if (!entity || !sdu_buffer) {
    LOG_E(SDAP, "%s:%d:%s: invalid args\n", __FILE__, __LINE__, __FUNCTION__);
    return false;
  }

  int offset = 0;
  bool ret = false;
  rb_id_t sdap_drb_id = rb_id;
  bool pdcp_ent_has_sdap = false;

  /* lookup mapping qfi -> drb */
  rb_id_t mapped = 0;
  if (entity->qfi2drb_map) mapped = entity->qfi2drb_map(entity, qfi);

  if (mapped) {
    sdap_drb_id = mapped;
    pdcp_ent_has_sdap = entity->qfi2drb_table[qfi].has_sdap_tx;
    LOG_D(SDAP, "TX - QFI: %u is mapped to DRB ID: %ld\n", qfi, (long)entity->qfi2drb_table[qfi].drb_id);
  }

  if (!pdcp_ent_has_sdap) {
    LOG_D(SDAP, "TX - DRB ID: %ld does not have SDAP, forwarding raw SDU\n", (long)sdap_drb_id);
    ret = nr_pdcp_data_req_drb(ctxt_p,
                               srb_flag,
                               sdap_drb_id,
                               mui,
                               confirm,
                               sdu_buffer_size,
                               sdu_buffer,
                               pt_mode,
                               sourceL2Id,
                               destinationL2Id);
    if (!ret) LOG_E(SDAP, "%s:%d:%s: PDCP refused PDU\n", __FILE__, __LINE__, __FUNCTION__);
    return ret;
  }

  /* enforce header length */
  offset = SDAP_HDR_LENGTH;
  if (sdu_buffer_size == 0 || sdu_buffer_size > 8999) {
    LOG_E(SDAP, "%s:%d:%s: invalid sdu_buffer_size %d\n", __FILE__, __LINE__, __FUNCTION__, sdu_buffer_size);
    return false;
  }

  uint8_t sdap_buf[SDAP_MAX_PDU];
  if (ctxt_p->enb_flag) { /* gNB - DL */
    nr_sdap_dl_hdr_t sdap_hdr;
    sdap_hdr.QFI = qfi;
    sdap_hdr.RQI = rqi ? 1 : 0;
    sdap_hdr.RDI = 0; /* typically 0 unless reflective mapping indicated */
    memcpy(&sdap_buf[0], &sdap_hdr, SDAP_HDR_LENGTH);
    memcpy(&sdap_buf[SDAP_HDR_LENGTH], sdu_buffer, sdu_buffer_size);
    LOG_D(SDAP, "TX(DL) SDAP QFI:%u RQI:%u RDI:%u to DRB %ld\n", sdap_hdr.QFI, sdap_hdr.RQI, sdap_hdr.RDI, (long)sdap_drb_id);
  } else { /* UE - UL */
    nr_sdap_ul_hdr_t sdap_hdr;
    sdap_hdr.QFI = qfi;
    sdap_hdr.R = 0;
    sdap_hdr.DC = rqi ? 1 : 0;
    memcpy(&sdap_buf[0], &sdap_hdr, SDAP_HDR_LENGTH);
    memcpy(&sdap_buf[SDAP_HDR_LENGTH], sdu_buffer, sdu_buffer_size);
    LOG_D(SDAP, "TX(UL) SDAP QFI:%u R:%u DC:%u to DRB %ld\n", sdap_hdr.QFI, sdap_hdr.R, sdap_hdr.DC, (long)sdap_drb_id);
  }

  /* submit to PDCP for transmission */
  ret = nr_pdcp_data_req_drb(ctxt_p,
                             srb_flag,
                             sdap_drb_id,
                             mui,
                             confirm,
                             sdu_buffer_size + offset,
                             sdap_buf,
                             pt_mode,
                             sourceL2Id,
                             destinationL2Id);

  if (!ret) LOG_E(SDAP, "%s:%d:%s: PDCP refused PDU\n", __FILE__, __LINE__, __FUNCTION__);
  return ret;
}

static void nr_sdap_rx_entity(nr_sdap_entity_t *entity,
                              rb_id_t pdcp_entity,
                              int is_gnb,
                              bool has_sdap_rx,
                              int pdusession_id,
                              ue_id_t ue_id,
                              char *buf,
                              int size)
{
  if (!entity || !buf) {
    LOG_E(SDAP, "%s:%d: invalid args\n", __FILE__, __LINE__);
    return;
  }

  int offset = 0;

  if (is_gnb) { /* UL: UE -> gNB */
    if (!has_sdap_rx) {
      LOG_D(SDAP, "RX - no SDAP header, forwarding whole payload to GTP-U\n");
      instance_t inst = *N3GTPUInst;
      gtpv1uSendDirect(inst, ue_id, pdusession_id, (uint8_t*)buf, (size_t)size, false, false);
      return;
    }

    if (size < SDAP_HDR_LENGTH) {
      LOG_W(SDAP, "RX - packet too small for SDAP header (size=%d), dropping\n", size);
      return;
    }

    nr_sdap_ul_hdr_t sdap_hdr;
    memcpy(&sdap_hdr, buf, SDAP_HDR_LENGTH);
    offset = SDAP_HDR_LENGTH;

    uint8_t qfi = sdap_hdr.QFI;
    LOG_D(SDAP, "RX Entity Received QFI: %u\n", qfi);

    /* USE CORRECT MACRO NAME */
    if (qfi == 0 || qfi >= SDAP_MAX_QFI) {
      LOG_W(SDAP, "RX - invalid or reserved QFI %u, forwarding default\n", qfi);
      instance_t inst = *N3GTPUInst;
      gtpv1uSendDirect(inst, ue_id, pdusession_id, (uint8_t*)(buf + offset), (size_t)(size - offset), false, false);
      return;
    }

    /* THREAD-SAFE READ OF MAPPING TABLE
       assume map_mutex is declared as: pthread_mutex_t map_mutex; */
    bool is_private = false;
    rb_id_t mapped_drb = 0;

    pthread_mutex_lock(&entity->map_mutex);
    is_private = entity->qfi2drb_table[qfi].is_private;
    mapped_drb = entity->qfi2drb_table[qfi].drb_id;
    pthread_mutex_unlock(&entity->map_mutex);

    if (is_private) {
      if (entity->tap_fd < 0) {
        LOG_E(SDAP, "Private QFI %u configured but tap_fd invalid (fd=%d)\n", qfi, entity->tap_fd);
        return;
      }

      LOG_I(SDAP, "Private QFI %u: writing %d bytes to TAP %s (fd=%d)\n",
             qfi, size - offset,
             entity->tap_ifname[0] ? entity->tap_ifname : "(null)", entity->tap_fd);

      ssize_t bytes_to_write = size - offset;
      ssize_t written_total = 0;
      const char *write_ptr = buf + offset;

      while (written_total < bytes_to_write) {
        ssize_t w = write(entity->tap_fd, write_ptr + written_total, (size_t)(bytes_to_write - written_total));
        if (w < 0) {
          if (errno == EINTR) continue;
          if (errno == EAGAIN || errno == EWOULDBLOCK) {
            LOG_W(SDAP, "Tap write EAGAIN, dropping remainder\n");
            break;
          }
          LOG_E(SDAP, "Failed to write TAP %s: %s\n",
                entity->tap_ifname[0] ? entity->tap_ifname : "(null)",
                strerror(errno));
          break;
        }
        written_total += w;
      }

      LOG_D(SDAP, "Private QFI %u: wrote %zd/%zd bytes to TAP\n", qfi, written_total, bytes_to_write);
      return;
    }

    /* Non-private path: forward to GTP-U */
    uint8_t *gtp_buf = (uint8_t *)(buf + offset);
    size_t gtp_len = (size_t)(size - offset);
    LOG_D(SDAP, "Forwarding SDAP SDU to GTP-U (len=%zu) qfi=%u mapped_drb=%ld\n", gtp_len, qfi, (long)mapped_drb);

    instance_t inst = *N3GTPUInst;
    gtpv1uSendDirect(inst, ue_id, pdusession_id, gtp_buf, gtp_len, false, false);
    return;
  } /* end is_gnb */

  /* ------------------ UE side (DL) ------------------ */
  else {
    if (has_sdap_rx) {
      if (size < SDAP_HDR_LENGTH) {
        LOG_W(SDAP, "UE RX - packet too small for SDAP header (size=%d)\n", size);
        return;
      }

      nr_sdap_dl_hdr_t sdap_hdr;
      memcpy(&sdap_hdr, buf, SDAP_HDR_LENGTH);
      offset = SDAP_HDR_LENGTH;

      LOG_D(SDAP, "RX Entity Received QFI : %u RQI:%u RDI:%u\n", sdap_hdr.QFI, sdap_hdr.RQI, sdap_hdr.RDI);

      /* Reflective mapping
         NOTE: use correct sdap_map_ctrl_pdu signature:
         rb_id_t sdap_map_ctrl_pdu(nr_sdap_entity_t *entity, uint8_t qfi, int map_type);
      */
      if (sdap_hdr.RDI == SDAP_REFLECTIVE_MAPPING) {
        LOG_D(SDAP, "RX - Performing Reflective Mapping for QFI %u\n", sdap_hdr.QFI);

        /* if no stored rule and default DRB exists -> send control PDU mapped to default */
        if (!entity->qfi2drb_table[sdap_hdr.QFI].drb_id && entity->default_drb) {
          nr_sdap_ul_hdr_t sdap_ctrl_pdu = entity->sdap_construct_ctrl_pdu(sdap_hdr.QFI);
          rb_id_t sdap_ctrl_pdu_drb = entity->sdap_map_ctrl_pdu(entity, sdap_hdr.QFI, SDAP_CTRL_PDU_MAP_DEF_DRB);
          entity->sdap_submit_ctrl_pdu(ue_id, sdap_ctrl_pdu_drb, sdap_ctrl_pdu);
        }

        if (pdcp_entity != entity->qfi2drb_table[sdap_hdr.QFI].drb_id) {
          nr_sdap_ul_hdr_t sdap_ctrl_pdu = entity->sdap_construct_ctrl_pdu(sdap_hdr.QFI);
          rb_id_t sdap_ctrl_pdu_drb =entity->sdap_map_ctrl_pdu(entity, sdap_hdr.QFI, SDAP_CTRL_PDU_MAP_RULE_DRB);

          /* NOTE: adjust call above to match your actual sdap_map_ctrl_pdu signature */
          entity->sdap_submit_ctrl_pdu(ue_id, sdap_ctrl_pdu_drb, sdap_ctrl_pdu);
        }

        /* store mapping */
        entity->qfi2drb_table[sdap_hdr.QFI].drb_id = pdcp_entity;
      }

      if (sdap_hdr.RQI == SDAP_RQI_HANDLING) {
        LOG_W(SDAP, "UE - RQI handling not implemented\n");
      }
    }

    extern int nas_sock_fd[];
    int to_write = size - offset;
    if (to_write <= 0) {
      LOG_W(SDAP, "UE RX - nothing to deliver to upper layer (size=%d offset=%d)\n", size, offset);
      return;
    }

    int len = write(nas_sock_fd[0], &buf[offset], to_write);
    LOG_D(SDAP, "RX Entity len : %d, expected : %d\n", len, to_write);
    if (len != to_write) {
      LOG_E(SDAP, "%s:%d:%s: failed to deliver to NAS (wrote %d != %d)\n", __FILE__, __LINE__, __FUNCTION__, len, to_write);
    }
    return;
  }
}

//zyzy: added
static int sdap_open_tap(const char *ifname)
{
  struct ifreq ifr;
  int fd = open("/dev/net/tun", O_RDWR);

  if (fd < 0) {
    LOG_E(SDAP, "sdap_open_tap: cannot open /dev/net/tun: %s\n", strerror(errno));
    return -1;
  }

  memset(&ifr, 0, sizeof(ifr));
  ifr.ifr_flags = IFF_TAP | IFF_NO_PI;   // TAP (L2), no extra packet info
  strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);

  if (ioctl(fd, TUNSETIFF, (void *)&ifr) < 0) {
    LOG_E(SDAP, "sdap_open_tap: TUNSETIFF %s failed: %s\n", ifname, strerror(errno));
    close(fd);
    return -1;
  }

  LOG_I(SDAP, "sdap_open_tap: TAP device %s opened (fd=%d)\n", ifname, fd);

  return fd;
}
static bool sdap_write_to_tap(nr_sdap_entity_t *entity,
                              const unsigned char *buf,
                              size_t len)
{
  if (!entity || !entity->use_tap) {
    LOG_W(SDAP, "sdap_write_to_tap: entity invalid or TAP disabled\n");
    return false;
  }

  pthread_mutex_lock(&entity->tap_lock);

  /* Lazy-open TAP if not already opened */
  if (entity->tap_fd < 0) {
    entity->tap_fd = sdap_open_tap(entity->tap_ifname);
    if (entity->tap_fd < 0) {
      pthread_mutex_unlock(&entity->tap_lock);
      return false;
    }
    LOG_I(SDAP, "sdap_write_to_tap: TAP %s is now active\n", entity->tap_ifname);
  }

  ssize_t wrote = write(entity->tap_fd, buf, len);
  if (wrote < 0) {
    LOG_E(SDAP, "sdap_write_to_tap: write to %s failed: %s\n",
          entity->tap_ifname, strerror(errno));
    pthread_mutex_unlock(&entity->tap_lock);
    return false;
  }

  LOG_D(SDAP, "sdap_write_to_tap: successfully wrote %zd bytes to %s\n",
        wrote, entity->tap_ifname);

  pthread_mutex_unlock(&entity->tap_lock);
  return true;
}

static size_t sdap_make_test_arp(uint8_t *packet,
                                 const uint8_t *src_mac,
                                 const char *src_ip,
                                 const char *dst_ip)
{
  uint8_t broadcast[6] = {0xff,0xff,0xff,0xff,0xff,0xff};

  /* Ethernet header */
  struct ethhdr *eth = (struct ethhdr *)packet;
  memcpy(eth->h_dest, broadcast, 6);
  memcpy(eth->h_source, src_mac, 6);
  eth->h_proto = htons(0x0806);  // ARP

  /* ARP header */
  ether_arp_fixed_t *arp = (ether_arp_fixed_t *)(packet + sizeof(struct ethhdr));

  arp->ea_hdr.ar_hrd = htons(1);      /* Ethernet */
  arp->ea_hdr.ar_pro = htons(0x0800); /* IPv4 */
  arp->ea_hdr.ar_hln = 6;
  arp->ea_hdr.ar_pln = 4;
  arp->ea_hdr.ar_op  = htons(1);      /* request */

  memcpy(arp->arp_sha, src_mac, 6);
  inet_pton(AF_INET, src_ip, arp->arp_spa);
  memset(arp->arp_tha, 0x00, 6);
  inet_pton(AF_INET, dst_ip, arp->arp_tpa);

  return sizeof(struct ethhdr) + sizeof(ether_arp_fixed_t);
}

bool sdap_test_connectivity(nr_sdap_entity_t *entity,
                            const char *cu_ip,
                            const char *ue_ip,
                            const uint8_t qfi
                            )
{
  if (!entity || !entity->use_tap) {
    LOG_E(SDAP, "sdap_test_connectivity: TAP disabled\n");
    return false;
  }

  if (entity->tap_fd < 0) {
    LOG_W(SDAP, "sdap_test_connectivity: TAP not opened, opening now...\n");
    entity->tap_fd = sdap_open_tap(entity->tap_ifname);
    if (entity->tap_fd < 0) {
      LOG_E(SDAP, "sdap_test_connectivity: unable to open TAP\n");
      return false;
    }
  }

  LOG_I(SDAP, "==== SDAP TAP CONNECTIVITY TEST ====\n");

  uint8_t mac_src[6] = {0x02,0x00,0x00,0x00,0x00,0x01}; // CU-UP MAC
  uint8_t pkt[1500];

  /* build ARP */
  size_t pkt_len = sdap_make_test_arp(pkt, mac_src, cu_ip, ue_ip);

  LOG_I(SDAP,
        "Sending ARP test packet: who-has %s tell %s over TAP %s (qfi=%u)\n",
        ue_ip, cu_ip, entity->tap_ifname, qfi);

  bool ok = sdap_write_to_tap(entity, pkt, pkt_len);

  if (!ok) {
    LOG_E(SDAP, "sdap_test_connectivity: TAP write failed\n");
    return false;
  }

  LOG_I(SDAP,
        "sdap_test_connectivity: TAP write OK. Run `tcpdump -ni %s` to confirm.\n",
        entity->tap_ifname);

  return true;
}

//zy-e
/** edited by zyzy
 * @brief update QFI to DRB mapping rules
*/
void nr_sdap_qfi2drb_map_update(nr_sdap_entity_t *entity, 
                                uint8_t qfi, 
                                rb_id_t drb, 
                                bool has_sdap_rx, 
                                bool has_sdap_tx,
                                bool is_private)
{
  if (!entity) {
    LOG_E(SDAP, "map_update called with NULL entity\n");
    return;
  }

  /* Validation */
  if (qfi == 0 || qfi >= SDAP_MAX_QFI) {
    LOG_W(SDAP, "map_update: invalid QFI %u\n", qfi);
    return;
  }

  if (drb == 0 || drb > AVLBL_DRB) {
    LOG_W(SDAP, "map_update: invalid DRB %ld for QFI %u\n", drb, qfi);
    return;
  }

  /* Lock table */
  pthread_mutex_lock(&entity->map_mutex);

  entity->qfi2drb_table[qfi].drb_id = drb;
  entity->qfi2drb_table[qfi].has_sdap_rx = has_sdap_rx;
  entity->qfi2drb_table[qfi].has_sdap_tx = has_sdap_tx;
  entity->qfi2drb_table[qfi].is_private = is_private;

  pthread_mutex_unlock(&entity->map_mutex);

  LOG_I(SDAP,
        "SDAP MAP UPDATE: QFI %u → DRB %ld  (rx=%d tx=%d private=%d)\n",
        qfi, drb, has_sdap_rx, has_sdap_tx, is_private);
}
//edited by zyzy
void nr_sdap_qfi2drb_map_del(nr_sdap_entity_t *entity, uint8_t qfi)
{
  if (!entity) {
    LOG_E(SDAP, "map_del called with NULL entity\n");
    return;
  }

  /* Boundary check */
  if (qfi == 0 || qfi >= SDAP_MAX_QFI) {
    LOG_W(SDAP, "map_del: invalid QFI %u\n", qfi);
    return;
  }

  /* Lock mapping access */
  pthread_mutex_lock(&entity->map_mutex);

  entity->qfi2drb_table[qfi].drb_id      = SDAP_NO_MAPPING_RULE;
  entity->qfi2drb_table[qfi].has_sdap_rx = false;
  entity->qfi2drb_table[qfi].has_sdap_tx = false;
  entity->qfi2drb_table[qfi].is_private  = false;

  pthread_mutex_unlock(&entity->map_mutex);

  LOG_I(SDAP, "Deleted SDAP mapping for QFI %u\n", qfi);
}

/** edited by zyzy
 * @brief   maps the QFIs to the default DRB if not mapping rule exists
 * @return  DRB that is mapped to the QFI, 0 if no mapping and no default DRB exists for that QFI
*/
rb_id_t nr_sdap_qfi2drb_map(nr_sdap_entity_t *entity, uint8_t qfi)
{
  if (!entity) {
    LOG_E(SDAP, "qfi2drb_map called with NULL entity\n");
    return SDAP_MAP_RULE_EMPTY;
  }

  /* Boundary protection */
  if (qfi == 0 || qfi >= SDAP_MAX_QFI) {
    LOG_W(SDAP, "qfi2drb_map: invalid QFI %u\n", qfi);
    return SDAP_MAP_RULE_EMPTY;
  }

  /* Thread-safe access */
  pthread_mutex_lock(&entity->map_mutex);

  rb_id_t mapped = entity->qfi2drb_table[qfi].drb_id;
  bool is_private = entity->qfi2drb_table[qfi].is_private;

  pthread_mutex_unlock(&entity->map_mutex);

  /* -------------- Case 1: Rule already exists -------------- */
  if (mapped > 0) {
    LOG_D(SDAP, "Mapping: QFI %u → DRB %ld\n", qfi, mapped);
    return mapped;
  }

  /* -------------- Case 2: QFI private → DO NOT MAP -------------- */
  if (is_private) {
    LOG_I(SDAP, "QFI %u is private → no DRB mapping\n", qfi);
    return SDAP_MAP_RULE_EMPTY;
  }

  /* -------------- Case 3: No rule → use default DRB (if available) -------------- */
  if (entity->default_drb > 0) {
    LOG_D(SDAP, "No mapping for QFI %u, using default DRB %ld\n",
          qfi, entity->default_drb);

    /* Default DRB always uses has_sdap_rx/tx = true */
    entity->qfi2drb_map_update(entity,
                               qfi,
                               entity->default_drb,
                               true,    /* has_sdap_rx */
                               true,    /* has_sdap_tx */
                               false);  /* private? → NO */

    return entity->default_drb;
  }

  /* -------------- Case 4: No rule and no default DRB -------------- */
  LOG_D(SDAP, "No mapping rule & no default DRB for QFI %u\n", qfi);
  return SDAP_MAP_RULE_EMPTY;
}

nr_sdap_ul_hdr_t nr_sdap_construct_ctrl_pdu(uint8_t qfi){
  nr_sdap_ul_hdr_t sdap_end_marker_hdr;
  sdap_end_marker_hdr.QFI = qfi;
  sdap_end_marker_hdr.R = 0;
  sdap_end_marker_hdr.DC = SDAP_HDR_UL_CTRL_PDU;
  LOG_D(SDAP, "Constructed Control PDU with QFI:%u R:%u DC:%u \n", sdap_end_marker_hdr.QFI,
                                                                   sdap_end_marker_hdr.R,
                                                                   sdap_end_marker_hdr.DC);
  return sdap_end_marker_hdr;
}

rb_id_t nr_sdap_map_ctrl_pdu(
    nr_sdap_entity_t *entity,
    uint8_t qfi,
    int map_type
) {
  rb_id_t drb = 0;

  if (!entity) {
    LOG_E(SDAP, "map_ctrl_pdu: null entity\n");
    return 0;
  }

  if (qfi >= SDAP_MAX_QFI) {
    LOG_E(SDAP, "map_ctrl_pdu: invalid QFI %u\n", qfi);
    return 0;
  }

  switch(map_type) {

    case SDAP_CTRL_PDU_MAP_DEF_DRB:
      drb = entity->default_drb;
      LOG_D(SDAP, "Control-PDU: map QFI %u → default DRB=%ld\n", qfi, drb);
      break;

    case SDAP_CTRL_PDU_MAP_RULE_DRB:
      drb = entity->qfi2drb_map(entity, qfi);
      LOG_D(SDAP, "Control-PDU: map QFI %u → rule DRB=%ld\n", qfi, drb);
      break;

    default:
      LOG_W(SDAP, "Control-PDU: unknown map_type=%d for QFI %u\n",
            map_type, qfi);
      drb = 0;
      break;
  }

  return drb;
}


/**
 * @brief Submit the end-marker control PDU to PDCP according to TS 37.324, clause 5.3
 */
void nr_sdap_submit_ctrl_pdu(ue_id_t ue_id, rb_id_t sdap_ctrl_pdu_drb, nr_sdap_ul_hdr_t ctrl_pdu)
{
  if(sdap_ctrl_pdu_drb){
    nr_pdcp_submit_sdap_ctrl_pdu(ue_id, sdap_ctrl_pdu_drb, ctrl_pdu);
    LOG_D(SDAP, "Sent Control PDU to PDCP Layer.\n");
  }
}

/** edited by zyzy
 * @brief UL QoS flow to DRB mapping configuration for an existing SDAP entity
 *        according to TS 37.324, 5.3 QoS flow to DRB Mapping, clause 5.3.1 Configuration Procedures
 */
static void nr_sdap_ue_qfi2drb_config(nr_sdap_entity_t *entity,
                                      rb_id_t drb_identity,
                                      ue_id_t ue_id,
                                      NR_QFI_t *qfi_list,
                                      uint8_t qfi_count,
                                      bool has_sdap_rx,
                                      bool has_sdap_tx,
                                      bool is_private)
{
  for (int i = 0; i < qfi_count; i++) {

    uint8_t qfi = qfi_list[i];

    /* Boundary check */
    if (qfi == 0 || qfi >= SDAP_MAX_QFI) {
      LOG_W(SDAP, "Invalid QFI %u skipped\n", qfi);
      continue;
    }

    rb_id_t old_drb = entity->qfi2drb_table[qfi].drb_id;

    /* CASE 1 — default DRB available AND no mapping rule exists */
    if (entity->default_drb &&
        old_drb == SDAP_NO_MAPPING_RULE)
    {
      nr_sdap_ul_hdr_t ctrl = nr_sdap_construct_ctrl_pdu(qfi);
      rb_id_t drb = nr_sdap_map_ctrl_pdu(entity, qfi, SDAP_CTRL_PDU_MAP_DEF_DRB);

      nr_sdap_submit_ctrl_pdu(ue_id, drb, ctrl);

      LOG_D(SDAP, "CTRL-PDU sent: default DRB for QFI %u old=%ld new=%ld\n",
            qfi, old_drb, drb_identity);
    }

    /* CASE 2 — mapping rule changed and SDAP TX active */
    else if (old_drb != drb_identity && has_sdap_tx)
    {
      nr_sdap_ul_hdr_t ctrl = nr_sdap_construct_ctrl_pdu(qfi);
      rb_id_t drb = nr_sdap_map_ctrl_pdu(entity, qfi, SDAP_CTRL_PDU_MAP_RULE_DRB);

      nr_sdap_submit_ctrl_pdu(ue_id, drb, ctrl);

      LOG_D(SDAP, "CTRL-PDU sent: mapping change for QFI %u old=%ld new=%ld\n",
            qfi, old_drb, drb_identity);
    }

    /* Now apply the new mapping */
    nr_sdap_qfi2drb_map_update(
      entity,
      qfi,
      drb_identity,
      has_sdap_rx,
      has_sdap_tx,
      is_private);

    LOG_D(SDAP, "Stored mapping: QFI %u → DRB %ld (private=%d)\n",
          qfi, drb_identity, is_private);
  }
}


/**
 * edited by zyzy
 * @brief   add a new SDAP entity according to 5.1.1. of 3GPP TS 37.324
 * @note    there is one SDAP entity per PDU session
 *
 * @param   is_gnb, indicates whether it is for gNB or UE
 * @param   has_sdap_rx, indicates whether it is a receiving SDAP entity
 * @param   has_sdap_tx, indicates whether it is a transmitting SDAP entity
 * @param   ue_id, UE ID
 * @param   pdusession_id, PDU session ID
 * @param   is_defaultDRB, indicates whether the entity has a default DRB
 * @param   mapped_qfi_2_add, list of QoS flows to add/update
 * @param   mappedQFIs2AddCount, number of QoS flows to add/update
 */
nr_sdap_entity_t *new_nr_sdap_entity(
        int is_gnb,
        bool has_sdap_rx,
        bool has_sdap_tx,
        ue_id_t ue_id,
        int pdusession_id,
        bool is_defaultDRB,
        uint8_t drb_identity,
        NR_QFI_t *mapped_qfi_2_add,
        uint8_t mappedQFIs2AddCount)
{
  /* Check if entity already exists */
  nr_sdap_entity_t *existing = nr_sdap_get_entity(ue_id, pdusession_id);
  if (existing) {
    LOG_I(SDAP, "SDAP Entity already exists for UE %lu PDU %d, updating mapping\n",
          ue_id, pdusession_id);

    rb_id_t drb = existing->default_drb ? existing->default_drb : drb_identity;

    for (int i = 0; i < mappedQFIs2AddCount; i++) {
      uint8_t q = (uint8_t)mapped_qfi_2_add[i];
      bool is_private = (q == 7);

      if (existing->qfi2drb_map_update)
        existing->qfi2drb_map_update(existing, q, drb, has_sdap_rx, has_sdap_tx, is_private);
    }
    return existing;
  }

  /* Allocate new SDAP entity */
  nr_sdap_entity_t *sdap = calloc(1, sizeof(nr_sdap_entity_t));
  if (!sdap) {
    LOG_E(SDAP, "Failed to allocate SDAP entity\n");
    exit(1);
  }

  /* Basic setup */
  sdap->ue_id = ue_id;
  sdap->pdusession_id = pdusession_id;
  sdap->default_drb = 0;

  sdap->tap_fd = -1;
  sdap->tap_ifname[0] = '\0';
  sdap->use_tap = false;

  /* INIT mutex (fix merah) */
  pthread_mutex_init(&sdap->map_mutex, NULL);

  /* Function pointers */
  sdap->tx_entity = nr_sdap_tx_entity;
  sdap->rx_entity = nr_sdap_rx_entity;

  sdap->sdap_construct_ctrl_pdu = nr_sdap_construct_ctrl_pdu;
  sdap->sdap_map_ctrl_pdu       = nr_sdap_map_ctrl_pdu;
  sdap->sdap_submit_ctrl_pdu    = nr_sdap_submit_ctrl_pdu;

  sdap->qfi2drb_map_update = nr_sdap_qfi2drb_map_update;
  sdap->qfi2drb_map_delete = nr_sdap_qfi2drb_map_del;
  sdap->qfi2drb_map        = nr_sdap_qfi2drb_map;

  /* Initialize QFI table (fix loop) */
  for (int i = 0; i < SDAP_MAX_QFI; i++) {
    sdap->qfi2drb_table[i].drb_id = 0;
    sdap->qfi2drb_table[i].is_private = false;
    sdap->qfi2drb_table[i].has_sdap_tx = false;
    sdap->qfi2drb_table[i].has_sdap_rx = false;
  }

  /* Handle default DRB + mapping */
  if (is_defaultDRB) {
    sdap->default_drb = drb_identity;

    LOG_I(SDAP, "Default DRB = %u for new SDAP entity\n", drb_identity);

    for (int i = 0; i < mappedQFIs2AddCount; i++) {
      uint8_t q = (uint8_t)mapped_qfi_2_add[i];
      bool is_private = (q == 7);

      sdap->qfi2drb_map_update(sdap, q, drb_identity, has_sdap_rx, has_sdap_tx, is_private);
    }
  }

  /* Insert into global list */
  sdap->next_entity = sdap_info.sdap_entity_llist;
  sdap_info.sdap_entity_llist = sdap;

  LOG_I(SDAP, "Created SDAP entity UE=%lu PDU=%d (default DRB=%ld)\n",
        ue_id, pdusession_id, sdap->default_drb);

  return sdap;
}

/**
 * @brief   Fetches the SDAP entity for the give PDU session ID.
 * @note    There is one SDAP entity per PDU session.
 * @return  The pointer to the SDAP entity if existing, NULL otherwise
 */
nr_sdap_entity_t *nr_sdap_get_entity(ue_id_t ue_id, int pdusession_id)
{
  nr_sdap_entity_t *cur = sdap_info.sdap_entity_llist;

  if (!cur)
    return NULL;

  while (cur) {
    /* Match found */
    if (cur->ue_id == ue_id && cur->pdusession_id == pdusession_id)
      return cur;
    cur = cur->next_entity;
  }

  /* Not found */
  LOG_D(SDAP,"SDAP Entity not found (UE=%lu, PDU Session=%d)\n", ue_id, pdusession_id);
  return NULL;
}

void nr_sdap_release_drb(ue_id_t ue_id, int drb_id, int pdusession_id)
{
  nr_sdap_entity_t *sdap = nr_sdap_get_entity(ue_id, pdusession_id);
  if (!sdap) {
    LOG_E(SDAP,
          "SDAP release DRB failed: entity not found (UE=%lu, PDU=%d)\n",
          ue_id, pdusession_id);
    return;
  }

  /* Lock if needed */
  pthread_mutex_lock(&sdap->map_mutex);

  LOG_I(SDAP,
        "Releasing DRB %d for UE=%lu PDU=%d\n",
        drb_id, ue_id, pdusession_id);

  /* Clear all QFI→DRB mapping that use this DRB */
  for (int qfi = 0; qfi < SDAP_MAX_QFI; qfi++) {
    if (sdap->qfi2drb_table[qfi].drb_id == drb_id) {

      LOG_D(SDAP,
            " - Removing mapping: QFI %d → DRB %d\n",
            qfi, drb_id);

      sdap->qfi2drb_table[qfi].drb_id = SDAP_NO_MAPPING_RULE;
      sdap->qfi2drb_table[qfi].has_sdap_rx = false;
      sdap->qfi2drb_table[qfi].has_sdap_tx = false;
      sdap->qfi2drb_table[qfi].is_private = false;
    }
  }

  /* If removed DRB is default DRB → reset default */
  if (sdap->default_drb == drb_id) {
    LOG_W(SDAP,
          "Default DRB %d removed — clearing default_drb\n", drb_id);
    sdap->default_drb = 0;
  }
    pthread_mutex_unlock(&sdap->map_mutex);
}

bool nr_sdap_delete_entity(ue_id_t ue_id, int pdusession_id)
{
  nr_sdap_entity_t *entityPtr  = sdap_info.sdap_entity_llist;
  nr_sdap_entity_t *entityPrev = NULL;
  bool ret = false;

  if (entityPtr == NULL) {
    LOG_E(SDAP, "No SDAP entities exist.\n");
    return false;
  }

  if (pdusession_id < 0 || pdusession_id >= NGAP_MAX_PDU_SESSION) {
    LOG_E(SDAP, "Invalid pdusession_id=%d (valid range 0..%d).\n",
          pdusession_id, NGAP_MAX_PDU_SESSION - 1);
    return false;
  }

  LOG_D(SDAP, "Deleting SDAP entity for UE %lx, PDU Session %d\n",
        ue_id, pdusession_id);

  while (entityPtr != NULL) {

    if (entityPtr->ue_id == ue_id &&
        entityPtr->pdusession_id == pdusession_id)
    {
      /* ----------------------------
         CLEANUP TAP INTERFACE
         ---------------------------- */
      if (entityPtr->tap_fd >= 0) {
        LOG_I(SDAP, "Closing TAP %s (fd=%d)\n",
              entityPtr->tap_ifname, entityPtr->tap_fd);

        if (close(entityPtr->tap_fd) < 0) {
          LOG_W(SDAP, "Warning: failed to close TAP fd=%d\n",
                entityPtr->tap_fd);
        }

        entityPtr->tap_fd = -1;
      }

      /* Destroy mutex (only if valid) */
      pthread_mutex_destroy(&entityPtr->tap_lock);

      /* ----------------------------
         CLEANUP QFI → DRB TABLE
         ---------------------------- */
      for (int i = 0; i < SDAP_MAX_QFI; i++)
        entityPtr->qfi2drb_table[i].drb_id = SDAP_NO_MAPPING_RULE;

      /* ----------------------------
         REMOVE FROM LINKED LIST
         ---------------------------- */
      if (entityPrev == NULL)
        sdap_info.sdap_entity_llist = entityPtr->next_entity;
      else
        entityPrev->next_entity = entityPtr->next_entity;

      free(entityPtr);

      LOG_D(SDAP, "SDAP entity removed for UE %lx (PDU Session %d)\n",
            ue_id, pdusession_id);

      ret = true;
      break;
    }

    entityPrev = entityPtr;
    entityPtr  = entityPtr->next_entity;
  }

  if (!ret)
    LOG_E(SDAP, "SDAP entity for UE %lx, PDU Session %d not found.\n",
          ue_id, pdusession_id);

  return ret;
}

bool nr_sdap_delete_ue_entities(ue_id_t ue_id)
{
  nr_sdap_entity_t *entityPtr  = sdap_info.sdap_entity_llist;
  nr_sdap_entity_t *entityPrev = NULL;
  bool found = false;

  if (entityPtr == NULL) {
    LOG_W(SDAP, "No SDAP entities exist. Nothing to delete for UE %lx\n", ue_id);
    return false;
  }

  LOG_D(SDAP, "Deleting all SDAP entities for UE %lx...\n", ue_id);
  while (entityPtr != NULL) {
    if (entityPtr->ue_id == ue_id)
    {
      /* ----------------------------
         CLEANUP TAP
         ---------------------------- */
      if (entityPtr->tap_fd >= 0) {
        LOG_I(SDAP, "Closing TAP %s (fd=%d)\n",
              entityPtr->tap_ifname, entityPtr->tap_fd);
        close(entityPtr->tap_fd);
      }

      pthread_mutex_destroy(&entityPtr->tap_lock);

      /* ----------------------------
         CLEANUP QFI→DRB table
         ---------------------------- */
      for (int i = 0; i < SDAP_MAX_QFI; i++)
        entityPtr->qfi2drb_table[i].drb_id = SDAP_NO_MAPPING_RULE;

      /* ----------------------------
         REMOVE FROM LINKED LIST
         ---------------------------- */
      if (entityPrev == NULL)
        sdap_info.sdap_entity_llist = entityPtr->next_entity;
      else
        entityPrev->next_entity = entityPtr->next_entity;

      nr_sdap_entity_t *toDelete = entityPtr;
      entityPtr = entityPtr->next_entity;

      free(toDelete);

      LOG_I(SDAP, "Deleted SDAP entity for UE %lx\n", ue_id);
      found = true;
      continue;   // entityPtr already advanced
    }

    /* UE_id mismatch → advance */
    entityPrev = entityPtr;
    entityPtr  = entityPtr->next_entity;
  }

  if (!found)
    LOG_W(SDAP, "No SDAP entities found for UE %lx\n", ue_id);

  return found;
}

/** edited by zyzy
 * @brief SDAP Entity reconfiguration at UE according to TS 37.324
 *        and triggered by RRC reconfiguration events according to clause 5.3.5.6.5 of TS 38.331.
 *        This function performs:
 *        - QoS flow to DRB mapping according to clause 5.3.1 of TS 37.324
 */
void nr_reconfigure_sdap_entity(
  NR_SDAP_Config_t *sdap_config,
  ue_id_t ue_id,
  int pdusession_id,
  int drb_id,
  bool is_private)
{
  bool is_gnb = false;

  /* fetch SDAP entity */
  nr_sdap_entity_t *sdap_entity = nr_sdap_get_entity(ue_id, pdusession_id);
  AssertError(sdap_entity != NULL,
              return,
              "Could not find SDAP Entity for UE ID: %lu PDU SESSION ID: %d\n",
              ue_id,
              pdusession_id);

  /* compute SDAP RX/TX flags for this config (UE side) */
  bool has_sdap_rx = false;
  bool has_sdap_tx = false;
  if (sdap_config) {
    has_sdap_rx = is_sdap_rx(is_gnb, sdap_config);
    has_sdap_tx = is_sdap_tx(is_gnb, sdap_config);
  }

  /* --- HANDLE mappedQoS_FlowsToAdd --- */
  if (sdap_config && sdap_config->mappedQoS_FlowsToAdd &&
      sdap_config->mappedQoS_FlowsToAdd->list.count > 0)
  {
    int count = sdap_config->mappedQoS_FlowsToAdd->list.count;

    uint8_t qfi_list[count];
    int real_count = 0;

    for (int i = 0; i < count; i++) {
      NR_QFI_t *ptr = (NR_QFI_t *) sdap_config->mappedQoS_FlowsToAdd->list.array[i];
      if (!ptr) {
        LOG_W(SDAP, "mappedQoS_FlowsToAdd: element %d is NULL, skip\n", i);
        continue;
      }

      long val = (long)*ptr;
      if (val <= 0 || val >= SDAP_MAX_QFI) {
        LOG_W(SDAP, "mappedQoS_FlowsToAdd: invalid QFI %ld at index %d, skip\n", val, i);
        continue;
      }

      qfi_list[real_count++] = (uint8_t) val;
    }

    if (real_count > 0) {

      /* APPLY MAPPING */
      nr_sdap_ue_qfi2drb_config(sdap_entity,
                                drb_id,
                                ue_id,
                                (NR_QFI_t *)qfi_list,
                                (uint8_t)real_count,
                                has_sdap_rx,
                                has_sdap_tx,
                                is_private);

      /* ============================
         CALL CONNECTIVITY TEST HERE
         ============================ */
      uint8_t test_qfi = qfi_list[0];
      sdap_test_connectivity(sdap_entity, "10.5.0.1", "10.5.0.2",test_qfi);

      LOG_I(SDAP, "[TEST] SDAP connectivity test triggered for UE %lu DRB %d\n",
            ue_id, drb_id);

    } else {
      LOG_D(SDAP, "No valid QFIs found in mappedQoS_FlowsToAdd for UE %lu PDU %d\n",
            ue_id, pdusession_id);
    }

  } else {
    LOG_D(SDAP, "No mappedQoS_FlowsToAdd present in SDAP_Config for UE %lu PDU %d\n",
          ue_id, pdusession_id);
  }

  /* --- HANDLE mappedQoS_FlowsToRelease --- */
  if (sdap_config && sdap_config->mappedQoS_FlowsToRelease &&
      sdap_config->mappedQoS_FlowsToRelease->list.count > 0)
  {
    int rcount = sdap_config->mappedQoS_FlowsToRelease->list.count;

    for (int i = 0; i < rcount; i++) {

      NR_QFI_t *ptr = (NR_QFI_t *) sdap_config->mappedQoS_FlowsToRelease->list.array[i];
      if (!ptr) {
        LOG_W(SDAP, "mappedQoS_FlowsToRelease: element %d is NULL, skip\n", i);
        continue;
      }

      long val = (long)*ptr;
      if (val <= 0 || val >= SDAP_MAX_QFI) {
        LOG_W(SDAP, "mappedQoS_FlowsToRelease: invalid QFI %ld at index %d, skip\n", val, i);
        continue;
      }

      uint8_t qfi = (uint8_t)val;

      if (sdap_entity->qfi2drb_map_delete)
        sdap_entity->qfi2drb_map_delete(sdap_entity, qfi);
      else
        LOG_W(SDAP, "No qfi2drb_map_delete() callback defined for SDAP entity\n");
    }
  } else {
    LOG_D(SDAP, "No mappedQoS_FlowsToRelease present in SDAP_Config for UE %lu PDU %d\n",
          ue_id, pdusession_id);
  }
}

