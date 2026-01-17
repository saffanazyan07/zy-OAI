#ifdef __cplusplus
extern "C" {
#endif
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netdb.h>
#include <openair3/ocp-gtpu/upef_z-agf2.h>
#include "common/utils/LOG/log.h"
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/if_ether.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>

#define GTP_GPDU                                             (255)

#if defined(HAVE_LINUX_IF_H)
#include <linux/if.h>
#endif

#ifdef HAVE_SYS_UIO_H
#include <sys/uio.h>
#endif

upef_t gtpu_tunnels[MAX_TUNNELS] = {
    {"192.168.60.77", "192.168.60.88", 2154, -1, -1, 0x12345678, PTHREAD_MUTEX_INITIALIZER}
};

pthread_mutex_t gtp_lock = PTHREAD_MUTEX_INITIALIZER;

//-------------------------------------------------------------------------------------//
// create gtpu tunnel for cu-agf
// edited by zyzy
//-------------------------------------------------------------------------------------//

//-------------------------- Edited and optimized by zyzy -----------------------------//
#ifndef IFF_TUN
#define IFF_TUN         0x0001
#endif

#ifndef IFF_NO_PI
#define IFF_NO_PI       0x1000
#endif

#ifndef TUNSETIFF
#define TUNSETIFF       _IOW('T', 202, int)
#endif

#define GTPU_PORT 2154
#define BUFFER_SIZE 65536
#define CMD_BUFFER_SIZE 256

// Global Variables
bool gtpu_initialized = false;

// Function to create TUN device
int create_tun_device(const char *dev) {
    struct ifreq ifr;
    int fd = open("/dev/net/tun", O_RDWR);
    if (fd < 0) {
        LOG_E(GTPU, "Opening /dev/net/tun failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, dev, IFNAMSIZ - 1);
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI;

    if (ioctl(fd, TUNSETIFF, (void *)&ifr) < 0) {
        LOG_E(GTPU, "TUNSETIFF failed: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    LOG_I(GTPU, "Created TUN device %s\n", dev);
    return fd;
}

// Forwarding packet to GTP-U
void forward_to_gtpu(upef_t *tunnel, char *packet, ssize_t len, uint32_t teid) {
  char buffer[BUFFER_SIZE];
  struct sockaddr_in remote_addr = {0};

  remote_addr.sin_family = AF_INET;
  remote_addr.sin_port = htons(tunnel->port);
  inet_pton(AF_INET, tunnel->remote_ip, &remote_addr.sin_addr);

  struct gtp_header *gtp = (struct gtp_header *)buffer;
  gtp->flags = 0x30;  
  gtp->message_type = GTP_GPDU;
  gtp->length = htons(len);
  gtp->teid = htonl(teid);

  memcpy(buffer + sizeof(struct gtp_header), packet, len);

  LOG_I(GTPU, "Forwarding %zd bytes via GTP-U to %s:%d, TEID: %08x\n",
        len, tunnel->remote_ip, tunnel->port, teid);

  ssize_t sent = sendto(tunnel->udp_sock, buffer, sizeof(struct gtp_header) + len, 0,
                        (struct sockaddr *)&remote_addr, sizeof(remote_addr));

  if (sent < 0) {
      LOG_E(GTPU, "Error sending GTP packet: %s\n", strerror(errno));
  } else {
      LOG_I(GTPU, "Sent %zd bytes via GTP-U\n", sent);
  }
}

// Processing GTP-U packet
void process_gtpu_packet(upef_t *tunnel, char *buffer, ssize_t len) {
  struct gtp_header *gtp = (struct gtp_header *)buffer;

  if (gtp->message_type == GTP_GPDU) {
      ssize_t payload_len = ntohs(gtp->length);
      ssize_t written = write(tunnel->tun_fd, buffer + sizeof(struct gtp_header), payload_len);
      if (written < 0) {
          LOG_E(GTPU, "Error writing to TUN device: %s\n", strerror(errno));
      } else {
          LOG_I(GTPU, "Wrote %zd bytes to TUN device\n", written);
      }
  } else {
      LOG_W(GTPU, "Unhandled GTP message type: %02x\n", gtp->message_type);
  }
}

void gtpu_packet_loop(upef_t *tunnel) {
  fd_set read_fds;
  struct sockaddr_in remote_addr, sender_addr;
  socklen_t addr_len = sizeof(sender_addr);
  char buffer[BUFFER_SIZE];

  memset(&remote_addr, 0, sizeof(remote_addr));
  remote_addr.sin_family = AF_INET;
  remote_addr.sin_port = htons(tunnel->port);
  inet_pton(AF_INET, tunnel->remote_ip, &remote_addr.sin_addr);

  LOG_I(GTPU, "[GTPU] Starting loop for tunnel %s:%d (TEID: %08X)\n",
        tunnel->remote_ip, tunnel->port, tunnel->teid);

  while (1) {
      FD_ZERO(&read_fds);
      FD_SET(tunnel->tun_fd, &read_fds);
      FD_SET(tunnel->udp_sock, &read_fds);

      int max_fd = (tunnel->tun_fd > tunnel->udp_sock ? tunnel->tun_fd : tunnel->udp_sock) + 1;
      int activity = select(max_fd, &read_fds, NULL, NULL, NULL);

      if (activity < 0 && errno != EINTR) {
          LOG_E(GTPU, "Select error: %s\n", strerror(errno));
          continue;
      }

      if (FD_ISSET(tunnel->tun_fd, &read_fds)) {
          ssize_t len = read(tunnel->tun_fd, buffer, BUFFER_SIZE);
          if (len > 0) {
              LOG_I(GTPU, "Received %zd bytes from TUN device\n", len);
              forward_to_gtpu(tunnel, buffer, len, tunnel->teid);
          }
      }

      if (FD_ISSET(tunnel->udp_sock, &read_fds)) {
          ssize_t len = recvfrom(tunnel->udp_sock, buffer, BUFFER_SIZE, 0,
                                 (struct sockaddr *)&sender_addr, &addr_len);
          if (len > 0) {
              LOG_I(GTPU, "Received %zd bytes from UDP socket (from %s:%d)\n",
                    len, inet_ntoa(sender_addr.sin_addr), ntohs(sender_addr.sin_port));
              process_gtpu_packet(tunnel, buffer, len);
          }
      }
  }
}

void *gtpu_packet_loop_thread(void *arg) {
  upef_t *tunnel = (upef_t *)arg;
  gtpu_packet_loop(tunnel);
  return NULL;
}

int initialize_gtpu_tunnel(upef_t *tunnel, int tunnel_index) {
  tunnel->udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (tunnel->udp_sock < 0) {
      LOG_E(GTPU, "Failed to create UDP socket\n");
      return -1;
  }

  struct sockaddr_in local_addr = {0};
  local_addr.sin_family = AF_INET;
  local_addr.sin_port = htons(tunnel->port);
  inet_pton(AF_INET, tunnel->local_ip, &local_addr.sin_addr);

  if (bind(tunnel->udp_sock, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
      LOG_E(GTPU, "Failed to bind UDP socket\n");
      close(tunnel->udp_sock);
      return -1;
  }

  char tun_name[16];
  snprintf(tun_name, sizeof(tun_name), "gtp-tun%d", tunnel_index);
  tunnel->tun_fd = create_tun_device(tun_name);
  if (tunnel->tun_fd < 0) {
      LOG_E(GTPU, "Failed to create TUN device\n");
      close(tunnel->udp_sock);
      return -1;
  }

  char cmd[CMD_BUFFER_SIZE];

  snprintf(cmd, sizeof(cmd), "ip addr add 10.50.0.%d/24 dev %s", 77 + tunnel_index, tun_name);
  if (system(cmd) != 0) {
      LOG_W(GTPU, "Command '%s' returned non-zero exit status.\n", cmd);
  }

  snprintf(cmd, sizeof(cmd), "ip link set %s up", tun_name);
  if (system(cmd) != 0) {
      LOG_W(GTPU, "Command '%s' returned non-zero exit status.\n", cmd);
  }

  LOG_I(GTPU, "Tunnel initialized: %s -> %s (TEID: %08X)\n",
        tunnel->local_ip, tunnel->remote_ip, tunnel->teid);

  pthread_t thread;
  pthread_create(&thread, NULL, gtpu_packet_loop_thread, tunnel);
  pthread_detach(thread);

  return 0;
}

 
int initialize_gtpu_system() {
  for (int i = 0; i < MAX_TUNNELS; i++) {
      if (initialize_gtpu_tunnel(&gtpu_tunnels[i], i) == 0) {
          LOG_I(GTPU, "Tunnel %d setup success\n", i);
      } else {
          LOG_E(GTPU, "Tunnel %d setup failed\n", i);
      }
  }
  gtpu_initialized = true;
  return 0;
}


//----------------------------edited by zyzy end---------------------------------------//

#ifdef __cplusplus
}
#endif