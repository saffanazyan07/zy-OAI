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

/***********************************************************************
*
* FILENAME    :  phy_frame_configuration_nr.c
*
* DESCRIPTION :  functions related to FDD/TDD configuration for NR
*                see TS 38.213 11.1 Slot configuration
*                and TS 38.331 for RRC configuration
*
************************************************************************/

#include "PHY/defs_nr_common.h"
#include "PHY/defs_gNB.h"
#include "SCHED_NR/phy_frame_config_nr.h"
#include "common/utils/nr/nr_common.h"

#ifdef NR_UNIT_TEST
  #define FILE_NAME                " "
  #define LINE_FILE                (0)
  #define NR_TST_PHY_PRINTF(...)   printf(__VA_ARGS__)
#else
  #define FILE_NAME                (__FILE__)
  #define LINE_FILE                (__LINE__)
  #define NR_TST_PHY_PRINTF(...)
#endif

/** @brief This function processes TDD dedicated configuration for NR
 *         by processing the tdd_slot_bitmap and period_cfg, and
 *         allocates memory and fills max_num_of_symbol_per_slot_list
 *         in the nfapi config request (cfg)
 *  @ref   see TS 38.213 11.1 Slot configuration
 *  @param cfg NR config request structure pointer
 *  @param fs TDD configuration pointer
 *  @returns void
 */
void set_tdd_config_nr(nfapi_nr_config_request_scf_t *cfg, frame_structure_t *fs)
{
  AssertFatal(fs != NULL, "frame_structure must be non-NULL");

  int slot_number = 0;
  int nb_slots_to_set = fs->numb_slots_frame;
  int slot_index = 0;
  tdd_period_config_t *pc = &fs->period_cfg;
  tdd_bitmap_t *tdd_slot_bitmap = pc->tdd_slot_bitmap;

  LOG_I(NR_PHY,
        "Set TDD Period Configuration: %d periods per frame, %d slots to be configured (%d DL, %d UL)\n",
        fs->numb_period_frame,
        nb_slots_to_set,
        pc->num_dl_slots,
        pc->num_ul_slots);

  // Allocate memory formax_num_of_symbol_per_slot_list items
  cfg->tdd_table.max_tdd_periodicity_list = calloc_or_fail(nb_slots_to_set, sizeof(nfapi_nr_max_tdd_periodicity_t));
  for (int slot = 0; slot < nb_slots_to_set; slot++) {
    nfapi_nr_max_tdd_periodicity_t *p_list = &cfg->tdd_table.max_tdd_periodicity_list[slot];
    p_list->max_num_of_symbol_per_slot_list =
        calloc_or_fail(NR_NUMBER_OF_SYMBOLS_PER_SLOT, sizeof(*p_list->max_num_of_symbol_per_slot_list));
    for (int sym = 0; sym < NR_NUMBER_OF_SYMBOLS_PER_SLOT; sym++) {
      // for each symbol, assign the TLV tag for usage when packing
      p_list->max_num_of_symbol_per_slot_list[sym].slot_config.tl.tag = NFAPI_NR_CONFIG_SLOT_CONFIG_TAG;
    }
  }

  // Loop through all periods to be configured
  while (slot_number != nb_slots_to_set) {
    slot_index = get_slot_idx_in_period(slot_number, fs);
    /* Fill max_tdd_periodicity_list */
    nfapi_nr_max_tdd_periodicity_t *list = &cfg->tdd_table.max_tdd_periodicity_list[slot_number];
    // FULL DOWNLINK SLOTS
    if (tdd_slot_bitmap[slot_index].slot_type == TDD_NR_DOWNLINK_SLOT) {
      for (int sym = 0; sym < NR_NUMBER_OF_SYMBOLS_PER_SLOT; sym++) {
        list->max_num_of_symbol_per_slot_list[sym % NR_NUMBER_OF_SYMBOLS_PER_SLOT].slot_config.value = 0;
        if ((sym + 1) % NR_NUMBER_OF_SYMBOLS_PER_SLOT == 0) {
          slot_number++;
          list += 1;
        }
      }
    }
    // FLEXIBLE SLOTS
    if (tdd_slot_bitmap[slot_index].slot_type == TDD_NR_MIXED_SLOT) {
      int nrofDLSymbolsInSlot = tdd_slot_bitmap[slot_index].num_dl_symbols;
      int nrofULSymbolsInSlot = tdd_slot_bitmap[slot_index].num_ul_symbols;
      AssertFatal(nrofDLSymbolsInSlot + nrofULSymbolsInSlot < NR_NUMBER_OF_SYMBOLS_PER_SLOT,
                  "illegal symbol configuration DL %d, UL %d\n",
                  nrofDLSymbolsInSlot,
                  nrofULSymbolsInSlot);
      // DL Symbols
      for (int sym = 0; sym < nrofDLSymbolsInSlot; sym++) {
        list->max_num_of_symbol_per_slot_list[sym].slot_config.value = 0;
      }
      // Flexible Symbols
      for (int sym = nrofDLSymbolsInSlot; sym < NR_NUMBER_OF_SYMBOLS_PER_SLOT - nrofULSymbolsInSlot; sym++) {
        list->max_num_of_symbol_per_slot_list[sym].slot_config.value = 2;
      }
      // UL Symbols
      for (int sym = NR_NUMBER_OF_SYMBOLS_PER_SLOT - nrofULSymbolsInSlot; sym < NR_NUMBER_OF_SYMBOLS_PER_SLOT; sym++) {
        list->max_num_of_symbol_per_slot_list[sym].slot_config.value = 1;
      }
      slot_number++;
      list += 1;
    }
    // FULL UPLINK SLOTS
    if (tdd_slot_bitmap[slot_index].slot_type == TDD_NR_UPLINK_SLOT) {
      for (int sym = 0; sym < NR_NUMBER_OF_SYMBOLS_PER_SLOT; sym++) {
        list->max_num_of_symbol_per_slot_list[sym % NR_NUMBER_OF_SYMBOLS_PER_SLOT].slot_config.value = 1;
        if ((sym + 1) % NR_NUMBER_OF_SYMBOLS_PER_SLOT == 0) {
          slot_number++;
          list += 1;
        }
      }
    }
  }
  // Dump 1 period on the output
  for (int s = 0; s < fs->numb_slots_period; s++) {
    if (tdd_slot_bitmap[s].slot_type == TDD_NR_MIXED_SLOT) {
      char flexi_slot[NR_NUMBER_OF_SYMBOLS_PER_SLOT + 1];
      memset(flexi_slot, 0, sizeof(flexi_slot));
      for (int symb = 0; symb < NR_NUMBER_OF_SYMBOLS_PER_SLOT; symb++) {
        uint8_t val = cfg->tdd_table.max_tdd_periodicity_list[s].max_num_of_symbol_per_slot_list[symb].slot_config.value;
        char *symb_type_s[] = {"D", "U", "F"};
        flexi_slot[symb] = *symb_type_s[val];
      }
      flexi_slot[NR_NUMBER_OF_SYMBOLS_PER_SLOT] = '\0';
      LOG_I(NR_PHY, "TDD period configuration: slot %d is FLEXIBLE: %s\n", s, flexi_slot);
    } else {
      LOG_I(NR_PHY,
            "TDD period configuration: slot %d is %s\n",
            s,
            tdd_slot_bitmap[s].slot_type == TDD_NR_DOWNLINK_SLOT ? "DOWNLINK" : "UPLINK");
    }
  }
}

/*******************************************************************
*
* NAME :         add_tdd_dedicated_configuration_nr
*
* PARAMETERS :   pointer to frame configuration
*
* OUTPUT:        table of uplink symbol for each slot for several frames
*
* RETURN :       0 if tdd has been properly configurated
*                -1 tdd configuration can not be done
*
* DESCRIPTION :  generate bit map for uplink symbol for each slot for several frames
*                see TS 38.213 11.1 Slot configuration
*
*********************************************************************/

void add_tdd_dedicated_configuration_nr(NR_DL_FRAME_PARMS *frame_parms, int slotIndex, int nrofDownlinkSymbols, int nrofUplinkSymbols) {
  TDD_UL_DL_SlotConfig_t *p_TDD_UL_DL_ConfigDedicated = frame_parms->p_TDD_UL_DL_ConfigDedicated;
  TDD_UL_DL_SlotConfig_t *p_previous_TDD_UL_DL_ConfigDedicated=NULL;
  int next = 0;

  while (p_TDD_UL_DL_ConfigDedicated != NULL) {
    p_previous_TDD_UL_DL_ConfigDedicated = p_TDD_UL_DL_ConfigDedicated;
    p_TDD_UL_DL_ConfigDedicated = (TDD_UL_DL_SlotConfig_t *)(p_TDD_UL_DL_ConfigDedicated->p_next_TDD_UL_DL_SlotConfig);
    next = 1;
  }

  p_TDD_UL_DL_ConfigDedicated = calloc( 1, sizeof(TDD_UL_DL_SlotConfig_t));

  //printf("allocate pt %p \n", p_TDD_UL_DL_ConfigDedicated);
  if (p_TDD_UL_DL_ConfigDedicated == NULL) {
    printf("Error test_frame_configuration: memory allocation problem \n");
    assert(0);
  }

  if (next == 0) {
    frame_parms->p_TDD_UL_DL_ConfigDedicated = p_TDD_UL_DL_ConfigDedicated;
  } else {
    p_previous_TDD_UL_DL_ConfigDedicated->p_next_TDD_UL_DL_SlotConfig = (TDD_UL_DL_SlotConfig_t *)p_TDD_UL_DL_ConfigDedicated;
  }

  p_TDD_UL_DL_ConfigDedicated->slotIndex = slotIndex;
  p_TDD_UL_DL_ConfigDedicated->nrofDownlinkSymbols = nrofDownlinkSymbols;
  p_TDD_UL_DL_ConfigDedicated->nrofUplinkSymbols = nrofUplinkSymbols;
}

/*******************************************************************
*
* NAME :         set_tdd_configuration_dedicated_nr
*
* PARAMETERS :   pointer to frame configuration
*
* OUTPUT:        table of uplink symbol for each slot for several frames
*
* RETURN :       0 if tdd has been properly configurated
*                -1 tdd configuration can not be done
*
* DESCRIPTION :  generate bit map for uplink symbol for each slot for several frames
*                see TS 38.213 11.1 Slot configuration
*
*********************************************************************/

int set_tdd_configuration_dedicated_nr(NR_DL_FRAME_PARMS *frame_parms) {
  TDD_UL_DL_SlotConfig_t *p_current_TDD_UL_DL_SlotConfig;
  p_current_TDD_UL_DL_SlotConfig = frame_parms->p_TDD_UL_DL_ConfigDedicated;
  NR_TST_PHY_PRINTF("\nSet tdd dedicated configuration\n ");

  while(p_current_TDD_UL_DL_SlotConfig != NULL) {
    int slot_index = p_current_TDD_UL_DL_SlotConfig->slotIndex;

    if (slot_index < frame_parms->slots_per_frame) {
      if (p_current_TDD_UL_DL_SlotConfig->nrofDownlinkSymbols != 0) {
        if (p_current_TDD_UL_DL_SlotConfig->nrofDownlinkSymbols == NR_TDD_SET_ALL_SYMBOLS) {
          if (p_current_TDD_UL_DL_SlotConfig->nrofUplinkSymbols == 0) {
            frame_parms->tdd_uplink_nr[slot_index] = NR_TDD_DOWNLINK_SLOT;
            NR_TST_PHY_PRINTF(" DL[%d] ", slot_index);
          } else {
            LOG_E(PHY,"set_tdd_configuration_dedicated_nr: tdd downlink & uplink symbol configuration is not supported \n");
            return (-1);
          }
        } else {
          LOG_E(PHY,"set_tdd_configuration_dedicated_nr: tdd downlink symbol configuration is not supported \n");
          return (-1);
        }
      } else if (p_current_TDD_UL_DL_SlotConfig->nrofUplinkSymbols != 0) {
        if (p_current_TDD_UL_DL_SlotConfig->nrofUplinkSymbols == NR_TDD_SET_ALL_SYMBOLS) {
          frame_parms->tdd_uplink_nr[slot_index] = NR_TDD_UPLINK_SLOT;
          NR_TST_PHY_PRINTF(" UL[%d] ", slot_index);
        } else {
          LOG_E(PHY,"set_tdd_configuration_dedicated_nr: tdd uplink symbol configuration is not supported \n");
          return (-1);
        }
      } else {
        LOG_E(PHY,"set_tdd_configuration_dedicated_nr: no tdd symbol configuration is specified \n");
        return (-1);
      }
    } else {
      LOG_E(PHY,"set_tdd_configuration_dedicated_nr: tdd slot index exceeds maximum value \n");
      return (-1);
    }

    p_current_TDD_UL_DL_SlotConfig = (TDD_UL_DL_SlotConfig_t *)(p_current_TDD_UL_DL_SlotConfig->p_next_TDD_UL_DL_SlotConfig);
  }

  NR_TST_PHY_PRINTF("\n");
  return (0);
}

/*******************************************************************
*
* NAME :         set_tdd_configuration
*
* PARAMETERS :   pointer to tdd common configuration
*                pointer to tdd common configuration2
*                pointer to tdd dedicated configuration
*
* OUTPUT:        table of uplink symbol for each slot for 2 frames
*
* RETURN :       0  if srs sequence has been successfully generated
*                -1 if sequence can not be properly generated
*
* DESCRIPTION :  generate bit map for uplink symbol for each slot for 2 frames
*                see TS 38.213 11.1 Slot configuration
*
*********************************************************************/

int get_next_downlink_slot(PHY_VARS_gNB *gNB, nfapi_nr_config_request_scf_t *cfg, int nr_frame, int nr_slot) {

  int slot = nr_slot;
  int frame = nr_frame;
  int slots_per_frame = gNB->frame_parms.slots_per_frame;
  while (true) {
    slot++;
    if (slot/slots_per_frame) frame++;
    slot %= slots_per_frame;
    int slot_type = nr_slot_select(cfg, frame, slot);
    if (slot_type == NR_DOWNLINK_SLOT || slot_type == NR_MIXED_SLOT) return slot;
    AssertFatal(frame < (nr_frame+2), "Something went worng. This shouldn't happen\n");
  }
}

int nr_slot_select(nfapi_nr_config_request_scf_t *cfg, int nr_frame, int nr_slot)
{
  (void) nr_frame;

  /* for FFD all slot can be considered as an uplink */
  int check_slot = 0;

  if (cfg->cell_config.frame_duplex_type.value == FDD) {
    return (NR_UPLINK_SLOT | NR_DOWNLINK_SLOT);
  }

  for (int symbol_count = 0; symbol_count < NR_NUMBER_OF_SYMBOLS_PER_SLOT; symbol_count++) {
    if (cfg->tdd_table.max_tdd_periodicity_list[nr_slot].max_num_of_symbol_per_slot_list[symbol_count].slot_config.value == 1) {
      check_slot++;
    }
  }

  if (check_slot == NR_NUMBER_OF_SYMBOLS_PER_SLOT) {
    return (NR_UPLINK_SLOT);
  }

  check_slot = 0;

  for (int symbol_count = 0; symbol_count < NR_NUMBER_OF_SYMBOLS_PER_SLOT; symbol_count++) {
    if (cfg->tdd_table.max_tdd_periodicity_list[nr_slot].max_num_of_symbol_per_slot_list[symbol_count].slot_config.value == 0) {
      check_slot++;
    }
  }

  if (check_slot == NR_NUMBER_OF_SYMBOLS_PER_SLOT) {
    return (NR_DOWNLINK_SLOT);
  } else {
    return (NR_MIXED_SLOT);
  }
}

/*******************************************************************
*
* NAME :         free_tdd_configuration_nr
*
* PARAMETERS :   pointer to frame configuration
*
* RETURN :       none
*
* DESCRIPTION :  free structure related to tdd configuration
*
*********************************************************************/

void free_tdd_configuration_nr(NR_DL_FRAME_PARMS *frame_parms) {
  TDD_UL_DL_configCommon_t *p_tdd_UL_DL_Configuration = frame_parms->p_tdd_UL_DL_Configuration;
  free_tdd_configuration_dedicated_nr(frame_parms);

  if (p_tdd_UL_DL_Configuration != NULL) {
    frame_parms->p_tdd_UL_DL_Configuration = NULL;
    free(p_tdd_UL_DL_Configuration);
  }

  for (int number_of_slot = 0; number_of_slot < NR_MAX_SLOTS_PER_FRAME; number_of_slot++) {
    frame_parms->tdd_uplink_nr[number_of_slot] = NR_TDD_DOWNLINK_SLOT;
  }
}

/*******************************************************************
*
* NAME :         free_tdd_configuration_dedicated_nr
*
* PARAMETERS :   pointer to frame configuration
*
* RETURN :       none
*
* DESCRIPTION :  free structure related to tdd dedicated configuration
*
*********************************************************************/

void free_tdd_configuration_dedicated_nr(NR_DL_FRAME_PARMS *frame_parms) {
  TDD_UL_DL_SlotConfig_t *p_current_TDD_UL_DL_ConfigDedicated = frame_parms->p_TDD_UL_DL_ConfigDedicated;
  TDD_UL_DL_SlotConfig_t *p_next_TDD_UL_DL_ConfigDedicated;
  int next = 0;

  if (p_current_TDD_UL_DL_ConfigDedicated != NULL) {
    do {
      if (p_current_TDD_UL_DL_ConfigDedicated->p_next_TDD_UL_DL_SlotConfig != NULL) {
        next = 1;
        p_next_TDD_UL_DL_ConfigDedicated =  (TDD_UL_DL_SlotConfig_t *)(p_current_TDD_UL_DL_ConfigDedicated->p_next_TDD_UL_DL_SlotConfig);
        p_current_TDD_UL_DL_ConfigDedicated->p_next_TDD_UL_DL_SlotConfig = NULL;
        //printf("free pt %p \n", p_current_TDD_UL_DL_ConfigDedicated);
        free(p_current_TDD_UL_DL_ConfigDedicated);
        p_current_TDD_UL_DL_ConfigDedicated = p_next_TDD_UL_DL_ConfigDedicated;
      } else {
        if (p_current_TDD_UL_DL_ConfigDedicated != NULL) {
          frame_parms->p_TDD_UL_DL_ConfigDedicated = NULL;
          //printf("free pt %p \n", p_current_TDD_UL_DL_ConfigDedicated);
          free(p_current_TDD_UL_DL_ConfigDedicated);
          next = 0;
        }
      }
    } while (next);
  }
}

void do_tdd_config_sim(PHY_VARS_gNB *gNB, int mu)
{
  frame_structure_t fs = {.is_tdd = true};
  fs.numb_slots_frame = (1 << mu) * NR_NUMBER_OF_SUBFRAMES_PER_FRAME;
  fs.numb_period_frame = get_nb_periods_per_frame(gNB->gNB_config.tdd_table.tdd_period.value);
  fs.numb_slots_period = fs.numb_slots_frame / fs.numb_period_frame;
  tdd_period_config_t *pc = &fs.period_cfg;
  // Set DL/UL slots
  switch (mu)
  {
  case 0:
    pc->num_dl_slots = 3;
    pc->num_ul_slots = 1;
    break;
  
  case 1:
    pc->num_dl_slots = 7;
    pc->num_ul_slots = 2;
    break;

  case 3:
    pc->num_dl_slots = 27;
    pc->num_ul_slots = 12;
    break;

  default:
    break;
  }
  for (int i = 0; i < pc->num_dl_slots; i++) {
    pc->tdd_slot_bitmap[i].slot_type = TDD_NR_DOWNLINK_SLOT;
    pc->tdd_slot_bitmap[i].num_dl_symbols = 0;
    pc->tdd_slot_bitmap[i].num_ul_symbols = 0;
  }
  for (int i = pc->num_dl_slots; i < fs.numb_slots_period - pc->num_ul_slots; i++) {
    pc->tdd_slot_bitmap[i].slot_type = TDD_NR_MIXED_SLOT;
    pc->tdd_slot_bitmap[i].num_dl_symbols = 6;
    pc->tdd_slot_bitmap[i].num_ul_symbols = 4;
  }
  for (int i = pc->num_ul_slots; i < fs.numb_slots_period; i++) {
    pc->tdd_slot_bitmap[i].slot_type = TDD_NR_UPLINK_SLOT;
    pc->tdd_slot_bitmap[i].num_dl_symbols = 0;
    pc->tdd_slot_bitmap[i].num_ul_symbols = 0;
  }
  set_tdd_config_nr(&gNB->gNB_config, &fs);
}
