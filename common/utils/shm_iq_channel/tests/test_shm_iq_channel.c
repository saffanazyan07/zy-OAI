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
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <threads.h>

#define SHM_CHANNEL_NAME "shm_iq_channel_test_file"
#include "shm_iq_channel.h"

int server(void);
void echo_client(void);

enum { MODE_SERVER, MODE_CLIENT, MODE_FORK };
int main(int argc, char *argv[])
{
  int mode = MODE_FORK;
  if (argc == 2) {
    if (strcmp(argv[1], "server") == 0) {
      mode = MODE_SERVER;
    } else if (strcmp(argv[1], "client") == 0) {
      mode = MODE_CLIENT;
    }
  }
  switch (mode) {
    case MODE_SERVER:
      return server();
      break;
    case MODE_CLIENT:
      echo_client();
      break;
    case MODE_FORK: {
      int pid = fork();
      if (pid == 0) {
        echo_client();
      } else {
        return server();
      }
    } break;
  }

  return 0;
}

// Test for 50 slots to check wrap around
const int num_slots = 50;
int produce_symbols(void *arg)
{
  ShmIQChannel *channel = (ShmIQChannel *)arg;
  for (int i = 0; i < num_slots; i++) {
    shm_iq_channel_produce_symbols(channel, 14);
    usleep(20000);
  }
  return 0;
}

int server(void)
{
  int number_of_slots_per_frame = 20;
  int num_ant_tx = 1;
  int num_ant_rx = 1;
  int num_sc = 12;
  ShmIQChannel *channel = shm_iq_channel_create(SHM_CHANNEL_NAME, num_sc, number_of_slots_per_frame, num_ant_tx, num_ant_rx);
  while (true) {
    if (shm_iq_channel_is_connected(channel)) {
      printf("Server connected\n");
      break;
    }
    printf("Waiting for client\n");
    sleep(1);
  }

  thrd_t producer_thread;
  if (thrd_create(&producer_thread, produce_symbols, channel) != thrd_success) {
    fprintf(stderr, "Failed to create producer thread\n");
    exit(1);
  }

  int num_total_errors = 0;
  uint64_t timestamp = 0;
  int iq_contents = 0;
  while (timestamp < num_slots * 14) {
    uint64_t new_symbols = shm_iq_channel_symbols_ready(channel);
    if (new_symbols > 0) {
      timestamp += new_symbols;
      uint64_t target_timestamp = timestamp + 14;
      int32_t *data = shm_iq_channel_get_slot_tx_ptr(channel, target_timestamp, 0);
      size_t write_size = num_sc * 14;
      for (int i = 0; i < write_size; i++) {
        data[i] = iq_contents;
      }

      if (timestamp > 14 * 3) {
        // Read back from client
        int reference = iq_contents - 3;
        uint64_t read_timestamp = timestamp - 14;
        const int32_t *iq_rx = shm_iq_channel_get_slot_rx_ptr(channel, read_timestamp, 0);
        int num_errors = 0;
        for (int i = 0; i < channel->data->num_sc * 14; i++) {
          if (iq_rx[i] != reference) {
            num_errors++;
          }
        }
        if (num_errors) {
          printf("Found errors = %d, value = %d, reference = %d\n", num_errors, iq_rx[0], reference);
        }
        num_total_errors += num_errors;
      }

      iq_contents++;
    }
    usleep(1000);
  }

  printf("Finished writing data\n");

  shm_iq_channel_destroy(channel);
  if (thrd_join(producer_thread, NULL) != thrd_success) {
    fprintf(stderr, "Failed to join producer thread\n");
    exit(1);
  }
  return num_total_errors;
}

void echo_client(void)
{
  ShmIQChannel *channel = shm_iq_channel_connect(SHM_CHANNEL_NAME, 10);
  while (true) {
    if (shm_iq_channel_is_connected(channel)) {
      printf("Echo client connected\n");
      break;
    }
    printf("Waiting for server\n");
    sleep(1);
  }

  uint64_t timestamp = 0;
  int iq_contents = 0;
  while (timestamp < num_slots * 14) {
    uint64_t new_symbols = shm_iq_channel_symbols_ready(channel);
    if (new_symbols > 0) {
      timestamp += new_symbols;
      // Server starts producing from second slot
      if (timestamp > 28) {
        uint64_t target_timestamp = timestamp - 14;
        const int32_t *iq = shm_iq_channel_get_slot_rx_ptr(channel, target_timestamp, 0);
        int num_errors = 0;
        for (int i = 0; i < channel->data->num_sc * 14; i++) {
          if (iq[i] != iq_contents) {
            num_errors++;
          }
        }
        if (num_errors) {
          printf("Found %d errors, value = %d, reference = %d\n", num_errors, iq[0], iq_contents);
        }
        iq_contents++;

        // Write back to server
        uint64_t write_timestamp = timestamp + 14;
        int32_t *iq_tx = shm_iq_channel_get_slot_tx_ptr(channel, write_timestamp, 0);
        for (int i = 0; i < channel->data->num_sc * 14; i++) {
          iq_tx[i] = iq_contents;
        }
      }
    }
    usleep(1000);
  }

  printf("Finished reading data\n");
  shm_iq_channel_destroy(channel);
}
