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
#ifndef _NR_SDAP_ENTITY_H_
#define _NR_SDAP_ENTITY_H_

/*
 * OpenAirInterface header for NR SDAP entity (extended for TAP / private-QFI)
 */

#include <assertions.h>
#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>
#include <net/if.h>      /* IFNAMSIZ */
#include <netinet/in.h>  /* inet_pton etc. if needed by impl */

#include "NR_QFI.h"
#include "NR_SDAP-Config.h"
#include "common/platform_constants.h"

#define SDAP_BITMASK_DC             (0x80)
#define SDAP_BITMASK_R              (0x40)
#define SDAP_BITMASK_QFI            (0x3F)
#define SDAP_BITMASK_RQI            (0x40)
#define SDAP_HDR_UL_DATA_PDU        (1)
#define SDAP_HDR_UL_CTRL_PDU        (0)
#define SDAP_HDR_LENGTH             (1)
#define SDAP_MAX_QFI                (64)
#define SDAP_MAP_RULE_EMPTY         (0)
#define AVLBL_DRB                   (5)
#define SDAP_NO_MAPPING_RULE        (0)
#define SDAP_REFLECTIVE_MAPPING     (1)
#define SDAP_RQI_HANDLING           (1)
#define SDAP_CTRL_PDU_MAP_DEF_DRB   (0)
#define SDAP_CTRL_PDU_MAP_RULE_DRB  (1)
#define SDAP_MAX_PDU                (9000)
#define SDAP_MAX_NUM_OF_ENTITIES    (MAX_DRBS_PER_UE * MAX_MOBILES_PER_ENB)
#define SDAP_MAX_UE_ID              (65536)
#define SDAP_IS_PRIVATE_QFI(entity, qfi) ((entity)->use_tap && (qfi) == (entity)->private_qfi)

/* External references */
extern instance_t *N3GTPUInst;

/* SDAP headers (packed bitfields) */
typedef struct nr_sdap_dl_hdr_s {
  uint8_t QFI:6;
  uint8_t RQI:1;
  uint8_t RDI:1;
} __attribute__((packed)) nr_sdap_dl_hdr_t;

typedef struct nr_sdap_ul_hdr_s {
  uint8_t QFI:6;
  uint8_t R:1;
  uint8_t DC:1;
} __attribute__((packed)) nr_sdap_ul_hdr_t;

/* QFI -> DRB mapping entry */
typedef struct qfi2drb_s {
  rb_id_t drb_id;
  bool    has_sdap_rx;
  bool    has_sdap_tx;
  bool    is_private;     /* true = p-QFI (route locally to TAP) */
} qfi2drb_t;

/* Forward declare because function pointer types need it */
typedef struct nr_sdap_entity_s nr_sdap_entity_t;

void nr_pdcp_submit_sdap_ctrl_pdu(ue_id_t ue_id,
                                  rb_id_t sdap_ctrl_pdu_drb,
                                  nr_sdap_ul_hdr_t ctrl_pdu);

/* SDAP entity */
typedef struct nr_sdap_entity_s {

  /* --- identification --- */
  ue_id_t ue_id;
  int pdusession_id;
  rb_id_t default_drb;

  /* --- TAP interface --- */
  bool use_tap;                 /* enable/disable TAP */
  int tap_fd;                   /* file descriptor */
  char tap_ifname[64];          /* tap name */
  pthread_mutex_t tap_lock;     /* protects tap_fd */

  /* --- QFI handling --- */
  uint8_t private_qfi;          /* which QFI is private */
  pthread_mutex_t map_mutex;    /* protect QFI map table */

  /* QFI→DRB map */
  qfi2drb_t qfi2drb_table[SDAP_MAX_QFI];

  /* --- TX/RX SDAP entities --- */
  bool (*tx_entity)(nr_sdap_entity_t *,
                    protocol_ctxt_t *,
                    const srb_flag_t,
                    const rb_id_t,
                    const mui_t,
                    const confirm_t,
                    const sdu_size_t,
                    unsigned char *const,
                    const pdcp_transmission_mode_t,
                    const uint32_t *,
                    const uint32_t *,
                    const uint8_t,
                    const bool);

  void (*rx_entity)(nr_sdap_entity_t *,
                    rb_id_t,
                    int,
                    bool,
                    int,
                    ue_id_t,
                    char *,
                    int);

  /* --- SDAP control PDU helpers --- */
  nr_sdap_ul_hdr_t (*sdap_construct_ctrl_pdu)(uint8_t qfi);

  rb_id_t (*sdap_map_ctrl_pdu)(nr_sdap_entity_t *,
                               uint8_t qfi,
                               int map_type);

  void (*sdap_submit_ctrl_pdu)(ue_id_t, rb_id_t, nr_sdap_ul_hdr_t);

  /* --- Mapping helpers --- */
  void (*qfi2drb_map_update)(nr_sdap_entity_t *,
                             uint8_t,
                             rb_id_t,
                             bool,
                             bool,
                             bool);

  void (*qfi2drb_map_delete)(nr_sdap_entity_t *,
                             uint8_t);

  rb_id_t (*qfi2drb_map)(nr_sdap_entity_t *,
                         uint8_t);

  struct nr_sdap_entity_s *next_entity;

} nr_sdap_entity_t;


/* QFI to DRB Mapping Related Functions */
void nr_sdap_qfi2drb_map_update(nr_sdap_entity_t *entity,
                                uint8_t qfi,
                                rb_id_t drb,
                                bool has_sdap_rx,
                                bool has_sdap_tx,
                                bool is_private);

void nr_sdap_qfi2drb_map_del(nr_sdap_entity_t *entity, uint8_t qfi);

rb_id_t nr_sdap_qfi2drb_map(nr_sdap_entity_t *entity, uint8_t qfi);

/* Control PDU helpers */
nr_sdap_ul_hdr_t nr_sdap_construct_ctrl_pdu(uint8_t qfi);
rb_id_t nr_sdap_map_ctrl_pdu(nr_sdap_entity_t *entity, uint8_t qfi, int map_type);
void nr_sdap_submit_ctrl_pdu(ue_id_t ue_id, rb_id_t sdap_ctrl_pdu_drb, nr_sdap_ul_hdr_t ctrl_pdu);

/* SDAP entity lifecycle */
nr_sdap_entity_t *new_nr_sdap_entity(
        int is_gnb,
        bool has_sdap_rx,
        bool has_sdap_tx,
        ue_id_t ue_id,
        int pdusession_id,
        bool is_defaultDRB,
        uint8_t drb_identity,
        NR_QFI_t *mapped_qfi_2_add,
        uint8_t mappedQFIs2AddCount);
nr_sdap_entity_t *nr_sdap_get_entity(ue_id_t ue_id, int pdusession_id);
void nr_sdap_release_drb(ue_id_t ue_id, int drb_id, int pdusession_id);
bool nr_sdap_delete_entity(ue_id_t ue_id, int pdusession_id);
bool nr_sdap_delete_ue_entities(ue_id_t ue_id);

/* TX/RX helpers */
bool is_sdap_rx(bool is_gnb, NR_SDAP_Config_t *sdap_config);
bool is_sdap_tx(bool is_gnb, NR_SDAP_Config_t *sdap_config);

/* Reconfigure helper (RRC -> SDAP) */
void nr_reconfigure_sdap_entity(NR_SDAP_Config_t *sdap_config,
                                ue_id_t ue_id,
                                int pdusession_id,
                                int drb_id,
                                bool is_private);

/* TAP / testing helpers (declare so other C files can call them if needed) */
//int sdap_open_tap(const char *ifname);
//bool sdap_write_to_tap(nr_sdap_entity_t *entity, const unsigned char *buf, size_t len);
//size_t sdap_make_test_arp(uint8_t *packet, const uint8_t *src_mac, const char *src_ip, const char *dst_ip);
bool sdap_test_connectivity(nr_sdap_entity_t *entity, const char *cu_ip, const char *ue_ip, const uint8_t qfi);

/* Optional: test-only helper to inject DHCP DISCOVER (implement in .c if needed) */
/* bool sdap_inject_dhcp_discover(nr_sdap_entity_t *entity, ...); */

#endif /* _NR_SDAP_ENTITY_H_ */
