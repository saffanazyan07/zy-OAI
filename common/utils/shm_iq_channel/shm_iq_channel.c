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

#include "shm_iq_channel.h"
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include "assertions.h"

#define SYMBOLS_IN_SLOT 14

static size_t calculate_buffer_size(int num_sc, int num_slots, int num_ant)
{
  size_t buffer_size = (num_sc * num_slots * SYMBOLS_IN_SLOT * num_ant) * sizeof(int32_t);
  return buffer_size;
}

static size_t calculate_total_size(ShmIQChannelData *data)
{
  return calculate_buffer_size(data->num_sc, data->number_of_slots_in_buffer, data->num_antennas_rx)
         + calculate_buffer_size(data->num_sc, data->number_of_slots_in_buffer, data->num_antennas_tx) + sizeof(ShmIQChannelData);
}

ShmIQChannel *shm_iq_channel_create(const char *name, int num_sc, int number_of_slots_in_buffer, int num_tx_ant, int num_rx_ant)
{
  ShmIQChannel *channel = malloc(sizeof(ShmIQChannel));
  strncpy(channel->name, name, sizeof(channel->name) - 1);
  // Create shared memory segment
  int fd = shm_open(name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
  if (fd == -1) {
    perror("shm_open");
    exit(1);
  }
  size_t tx_buffer_size = calculate_buffer_size(num_sc, number_of_slots_in_buffer, num_rx_ant);
  size_t total_size =
      tx_buffer_size + calculate_buffer_size(num_sc, number_of_slots_in_buffer, num_tx_ant) + sizeof(ShmIQChannelData);

  // Set the size of the shared memory segment
  int res = ftruncate(fd, total_size);
  if (res == -1) {
    perror("ftruncate");
    exit(1);
  }

  // Map shared memory segment to address space
  ShmIQChannelData *shm_ptr = mmap(0, total_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (shm_ptr == MAP_FAILED) {
    perror("mmap");
    exit(1);
  }

  // Initialize shared memory
  memset(shm_ptr, 0, total_size);
  shm_ptr->num_sc = num_sc;
  shm_ptr->number_of_slots_in_buffer = number_of_slots_in_buffer;
  shm_ptr->num_antennas_tx = num_tx_ant;
  shm_ptr->num_antennas_rx = num_rx_ant;
  shm_ptr->current_symbol_index = 0;
  shm_ptr->is_connected = false;
  channel->tx_iq_data = (int32_t *)(shm_ptr + 1);
  channel->rx_iq_data = channel->tx_iq_data + tx_buffer_size / sizeof(int32_t);
  channel->data = shm_ptr;
  channel->type = IQ_CHANNEL_TYPE_SERVER;
  channel->last_read_symbol_index = 0;
  pthread_mutexattr_t mutex_attr;
  pthread_condattr_t cond_attr;
  pthread_mutexattr_init(&mutex_attr);
  pthread_condattr_init(&cond_attr);
  pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
  pthread_condattr_setpshared(&cond_attr, PTHREAD_PROCESS_SHARED);
  pthread_mutex_init(&shm_ptr->mutex, &mutex_attr);
  pthread_cond_init(&shm_ptr->cond, &cond_attr);
  shm_ptr->magic = SHM_MAGIC_NUMBER;
  return channel;
}

ShmIQChannel *shm_iq_channel_connect(const char *name, int timeout_in_seconds)
{
  ShmIQChannel *channel = malloc(sizeof(ShmIQChannel));
  // Create shared memory segment
  int fd = -1;
  while (timeout_in_seconds > 0 && fd == -1) {
    fd = shm_open(name, O_RDWR, S_IRUSR | S_IWUSR);
    timeout_in_seconds--;
    printf("Waiting for server to create shared memory segment\n");
    sleep(1);
  }
  if (fd == -1) {
    perror("shm_open");
    exit(1);
  }
  struct stat buf;
  fstat(fd, &buf);
  size_t total_size = buf.st_size;

  // Map shared memory segment to address space
  ShmIQChannelData *shm_ptr = mmap(0, total_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (shm_ptr == MAP_FAILED) {
    perror("mmap");
    exit(1);
  }

  channel->data = shm_ptr;
  channel->tx_iq_data = (int32_t *)(shm_ptr + 1);
  size_t tx_buffer_size = calculate_buffer_size(shm_ptr->num_sc, shm_ptr->number_of_slots_in_buffer, shm_ptr->num_antennas_rx);
  channel->rx_iq_data = channel->tx_iq_data + tx_buffer_size / sizeof(int32_t);
  channel->type = IQ_CHANNEL_TYPE_CLIENT;
  channel->last_read_symbol_index = 0;
  while (shm_ptr->magic != SHM_MAGIC_NUMBER) {
    printf("Waiting for server to initialize shared memory\n");
    sleep(1);
  }
  shm_ptr->is_connected = true;
  return channel;
}

int32_t *shm_iq_channel_get_slot_tx_ptr(ShmIQChannel *channel, uint64_t timestamp, int antenna)
{
  ShmIQChannelData *data = channel->data;
  if (data->is_connected == false) {
    return NULL;
  }
  // timestamp in the past
  uint64_t current_symbol_index = data->current_symbol_index;
  if (timestamp < current_symbol_index) {
    return NULL;
  }
  // Not reading at slot boundary
  if (timestamp % SYMBOLS_IN_SLOT != 0) {
    return NULL;
  }
  // timestamp is too far in the future
  if (timestamp - current_symbol_index >= data->number_of_slots_in_buffer * SYMBOLS_IN_SLOT) {
    return NULL;
  }

  // Data layout: SC x 14 symbols x Antennas x num_slots_in_buffer.
  // This means contiguous slot buffer for each antenna
  int slot_index = timestamp / SYMBOLS_IN_SLOT;
  int slot_size = data->num_sc * SYMBOLS_IN_SLOT;
  int slot_index_in_buffer = slot_index % data->number_of_slots_in_buffer;

  int antbuf_size;
  int32_t *base_ptr;
  if (channel->type == IQ_CHANNEL_TYPE_CLIENT) {
    antbuf_size = slot_size * data->num_antennas_tx;
    base_ptr = channel->rx_iq_data;
  } else {
    antbuf_size = slot_size * data->num_antennas_rx;
    base_ptr = channel->tx_iq_data;
  }

  int offset = slot_index_in_buffer * antbuf_size + antenna * slot_size;

  return base_ptr + offset;
}

const int32_t *shm_iq_channel_get_slot_rx_ptr(ShmIQChannel *channel, uint64_t timestamp, int antenna)
{
  ShmIQChannelData *data = channel->data;
  if (data->is_connected == false) {
    return NULL;
  }
  // timestamp in the future
  uint64_t current_symbol_index = data->current_symbol_index;
  if (timestamp > current_symbol_index) {
    return NULL;
  }
  // Not reading at slot boundary
  if (timestamp % SYMBOLS_IN_SLOT != 0) {
    return NULL;
  }
  // timestamp is too far in the past
  if (current_symbol_index - timestamp >= data->number_of_slots_in_buffer * SYMBOLS_IN_SLOT) {
    return NULL;
  }

  // Data layout: SC x 14 symbols x Antennas x num_slots_in_buffer.
  // This means contiguous slot buffer for each antenna
  int slot_index = timestamp / SYMBOLS_IN_SLOT;
  int slot_size = data->num_sc * SYMBOLS_IN_SLOT;
  int slot_index_in_buffer = slot_index % data->number_of_slots_in_buffer;

  int antbuf_size;
  int32_t *base_ptr;
  if (channel->type == IQ_CHANNEL_TYPE_CLIENT) {
    antbuf_size = slot_size * data->num_antennas_rx;
    base_ptr = channel->tx_iq_data;
  } else {
    antbuf_size = slot_size * data->num_antennas_tx;
    base_ptr = channel->rx_iq_data;
  }

  int offset = slot_index_in_buffer * antbuf_size + antenna * slot_size;

  return base_ptr + offset;
}

void shm_iq_channel_produce_symbols(ShmIQChannel *channel, int num_symbols)
{
  ShmIQChannelData *data = channel->data;
  if (channel->type != IQ_CHANNEL_TYPE_SERVER) {
    return;
  }
  if (data->is_connected == false) {
    return;
  }
  data->current_symbol_index += num_symbols;
}

size_t shm_iq_channel_symbols_ready(ShmIQChannel *channel)
{
  ShmIQChannelData *data = channel->data;
  if (data->is_connected == false) {
    return 0;
  }
  uint64_t current_symbol_index = data->current_symbol_index;
  uint64_t diff = current_symbol_index - channel->last_read_symbol_index;
  channel->last_read_symbol_index = current_symbol_index;
  return diff;
}

bool shm_iq_channel_is_connected(ShmIQChannel *channel)
{
  return channel->data->is_connected;
}

void shm_iq_channel_destroy(ShmIQChannel *channel)
{
  ShmIQChannelData *data = channel->data;
  munmap(data, calculate_total_size(data));
  if (channel->type == IQ_CHANNEL_TYPE_SERVER) {
    shm_unlink(channel->name);
  }
  free(channel);
}
