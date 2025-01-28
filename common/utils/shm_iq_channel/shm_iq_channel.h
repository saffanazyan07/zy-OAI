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
#ifndef SHM_IQ_CHANNEL_H
#define SHM_IQ_CHANNEL_H

#include "../threadPool/pthread_utils.h"
#include <stdint.h>
#include <stdbool.h>
#include <semaphore.h>

#define SHM_MAGIC_NUMBER 0x12345678

/**
 * ShmIqChannel is a shared memory bidirectional IQ channel with a single clock source.
 * The server (clock source) shall create the channel while the client should connect to
 * it.
 *
 * To write samples, simply write to the pointer returned by shm_iq_channel_get_slot_tx_ptr
 * To read samples, read IQ data from the pointer returned by shm_iq_channel_get_slot_rx_ptr
 * To indicate that samples are ready to be read by the client, call shm_iq_channel_produce_symbols
 * (server only)
 *
 * The timestamps used in the API are in symbols from the start of the channel
 */

typedef enum IQChannelType { IQ_CHANNEL_TYPE_SERVER, IQ_CHANNEL_TYPE_CLIENT } IQChannelType;

typedef struct {
  int magic;
  int num_sc;
  int number_of_slots_in_buffer;
  int num_antennas_tx;
  int num_antennas_rx;
  uint64_t current_symbol_index;
  bool is_connected;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
} ShmIQChannelData;

typedef struct {
  IQChannelType type;
  ShmIQChannelData *data;
  uint64_t last_read_symbol_index;
  char name[256];
  int32_t *tx_iq_data;
  int32_t *rx_iq_data;
} ShmIQChannel;

/**
 * @brief Creates a shared memory IQ channel.
 *
 * @param name The name of the shared memory segment.
 * @param num_sc The number of subcarriers.
 * @param number_of_slots_in_buffer The number of slots in the buffer.
 * @param num_tx_ant The number of TX antennas.
 * @param num_rx_ant The number of RX antennas.
 * @return A pointer to the created ShmIQChannel structure.
 */
ShmIQChannel *shm_iq_channel_create(const char *name, int num_sc, int number_of_slots_in_buffer, int num_tx_ant, int num_rx_ant);

/**
 * @brief Connects to an existing shared memory IQ channel.
 *
 * @param name The name of the shared memory segment.
 * @param timeout_in_seconds The timeout in seconds for the connection attempt.
 * @return A pointer to the connected ShmIQChannel structure.
 */
ShmIQChannel *shm_iq_channel_connect(const char *name, int timeout_in_seconds);

/**
 * @brief Gets the pointer to the TX IQ data slot for a given timestamp and antenna.
 *
 * @param channel The ShmIQChannel structure.
 * @param timestamp The timestamp for which to get the TX IQ data slot.
 * @param antenna The antenna index.
 * @return A pointer to the TX IQ data slot.
 */
int32_t *shm_iq_channel_get_slot_tx_ptr(ShmIQChannel *channel, uint64_t timestamp, int antenna);

/**
 * @brief Gets the pointer to the RX IQ data slot for a given timestamp and antenna.
 *
 * @param channel The ShmIQChannel structure.
 * @param timestamp The timestamp for which to get the RX IQ data slot.
 * @param antenna The antenna index.
 * @return A pointer to the RX IQ data slot.
 */
const int32_t *shm_iq_channel_get_slot_rx_ptr(ShmIQChannel *channel, uint64_t timestamp, int antenna);

/**
 * @brief Produces a specified number of symbols in the IQ channel.
 *
 * @param channel The ShmIQChannel structure.
 * @param num_symbols The number of symbols to produce.
 */
void shm_iq_channel_produce_symbols(ShmIQChannel *channel, int num_symbols);

/**
 * @brief Gets the number of symbols ready in the IQ channel.
 *
 * @param channel The ShmIQChannel structure.
 * @return The number of symbols ready.
 */
size_t shm_iq_channel_symbols_ready(ShmIQChannel *channel);

/**
 * @brief Checks if the IQ channel is connected.
 *
 * @param channel The ShmIQChannel structure.
 * @return True if the channel is connected, false otherwise.
 */
bool shm_iq_channel_is_connected(ShmIQChannel *channel);

/**
 * @brief Destroys the shared memory IQ channel.
 *
 * @param channel The ShmIQChannel structure.
 */
void shm_iq_channel_destroy(ShmIQChannel *channel);

#endif
