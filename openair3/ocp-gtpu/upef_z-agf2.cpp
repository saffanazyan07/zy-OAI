#ifdef __cplusplus
extern "C" {
#endif
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netdb.h>
#include <openair3/ocp-gtpu/gtp_itf_z-agf2.h>
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


struct gtp_header {
  uint8_t flags;
  uint8_t message_type;
  uint16_t length;
  uint32_t teid;
};

upef_t gtpu_tunnels[MAX_TUNNELS] = {
    {"192.168.60.91", "192.168.60.24", 2154, -1, -1, 0x12345678, PTHREAD_MUTEX_INITIALIZER}
};

pthread_mutex_t gtp_lock = PTHREAD_MUTEX_INITIALIZER;
bool gtpu_initialized = false;


// Function to create TUN device
int create_tun_device(const char *dev) {
  struct ifreq ifr;
  int fd = open("/dev/net/tun", O_RDWR);
  if (fd < 0) return -1;

  memset(&ifr, 0, sizeof(ifr));
  strncpy(ifr.ifr_name, dev, IFNAMSIZ - 1);
  ifr.ifr_flags = IFF_TUN | IFF_NO_PI;

  if (ioctl(fd, TUNSETIFF, &ifr) < 0) {
      close(fd);
      return -1;
  }

  return fd;
}

// Forwarding packet to GTP-U
void forward_to_gtpu(upef_t *tunnel, char *packet, ssize_t len, uint32_t teid) {
  char buffer[BUFFER_SIZE];
  struct gtp_header *gtp = (struct gtp_header *)buffer;

  // Proper GTP-U header according to standards (GTPv1-U)
  gtp->flags = 0x30;  // Version 1, GTP protocol
  gtp->message_type = 0xff;  // T-PDU (Payload IP packet)
  gtp->length = htons(len);
  gtp->teid = htonl(teid);

  memcpy(buffer + sizeof(struct gtp_header), packet, len);

  // Debugging: Print GTP header in hex format
  char hex[256] = {0};
  char *p = hex;

  for (size_t i = 0; i < sizeof(struct gtp_header); i++) {
    p += sprintf(p, "%02x ", (unsigned char)buffer[i]);
  }

  LOG_I(UPEF, "Header: %s\n", hex);

  // Initialize remote address explicitly
  struct sockaddr_in remote_addr = {0};
  remote_addr.sin_family = AF_INET;
  remote_addr.sin_port = htons(tunnel->port);
  inet_pton(AF_INET, tunnel->remote_ip, &remote_addr.sin_addr);

  ssize_t sent = sendto(tunnel->udp_sock, buffer, sizeof(struct gtp_header) + len, 0,
                        (struct sockaddr *)&remote_addr, sizeof(remote_addr));

  if (sent < 0) {
      LOG_E(GTPU, "Error sending GTP packet: %s\n", strerror(errno));
  } else {
      LOG_I(UPEF, "Sent %zd bytes via GTP-U to %s:%d (TEID: %08X)\n",
            sent, tunnel->remote_ip, tunnel->port, teid);
  }
}


// Processing GTP-U packet
void process_gtpu_packet(upef_t *tunnel, char *buffer, ssize_t len) {
  if (len < (ssize_t)sizeof(struct gtp_header)) {
      LOG_W(GTPU, "GTP packet too short!\n");
      return;
  }

  struct gtp_header *gtp = (struct gtp_header *)buffer;

  if (gtp->message_type == 0xff) { // T-PDU
      ssize_t payload_len = ntohs(gtp->length);
      if ((size_t)(payload_len + sizeof(struct gtp_header)) > (size_t)len) {
          LOG_W(GTPU, "Invalid GTP payload length!\n");
          return;
      }

      ssize_t written = write(tunnel->tun_fd, buffer + sizeof(struct gtp_header), payload_len);
      if (written < 0) {
          LOG_E(GTPU, "Error writing to TUN device: %s\n", strerror(errno));
      } else {
          LOG_I(UPEF, "Wrote %zd bytes to TUN device (from GTP-U)\n", written);
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

  LOG_I(UPEF, "Starting GTP-U loop for tunnel: %s:%d (TEID: %08X)\n",
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

      // Handle TUN → UDP
      if (FD_ISSET(tunnel->tun_fd, &read_fds)) {
          ssize_t len = read(tunnel->tun_fd, buffer, BUFFER_SIZE);
          if (len > 0) {
              LOG_I(UPEF, "TUN received %zd bytes, sending via UDP (TEID: %08X)\n", len, tunnel->teid);
              forward_to_gtpu(tunnel, buffer, len, tunnel->teid);
          } else if (len < 0) {
              LOG_E(GTPU, "TUN read error: %s\n", strerror(errno));
          }
      }

      // Handle UDP → TUN (ini yang kurang di VM 88 kamu)
      if (FD_ISSET(tunnel->udp_sock, &read_fds)) {
          ssize_t len = recvfrom(tunnel->udp_sock, buffer, BUFFER_SIZE, 0,
                                 (struct sockaddr *)&sender_addr, &addr_len);
          if (len > 0) {
              LOG_I(UPEF, "Received %zd bytes from UDP socket (from %s:%d)\n",
                    len, inet_ntoa(sender_addr.sin_addr), ntohs(sender_addr.sin_port));
              process_gtpu_packet(tunnel, buffer, len);
          } else if (len < 0 && errno != EAGAIN) {
              LOG_E(GTPU, "UDP recvfrom error: %s\n", strerror(errno));
          }
      }
  }
}

void *gtpu_packet_loop_thread(void *arg) {
  gtpu_packet_loop((upef_t *)arg);
  return NULL;
}

void start_gtpu_threads() {
  pthread_t threads[MAX_TUNNELS];

  for (int i = 0; i < MAX_TUNNELS; i++) {
      if (pthread_create(&threads[i], NULL, gtpu_packet_loop_thread, &gtpu_tunnels[i]) != 0) {
          perror("Failed to create GTP-U thread");
      }
  }
}

int initialize_gtpu_tunnel(upef_t *tunnel, int tunnel_index) {
  LOG_I(UPEF, "Initializing GTP-U tunnel %d: %s -> %s (Port: %d, TEID: %08X)\n",
        tunnel_index, tunnel->local_ip, tunnel->remote_ip, tunnel->port, tunnel->teid);

  // --- Step 1: Buat UDP socket ---
  tunnel->udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (tunnel->udp_sock < 0) {
      LOG_E(GTPU, "Failed to create UDP socket: %s\n", strerror(errno));
      return -1;
  }

  struct sockaddr_in local_addr = {0};
  local_addr.sin_family = AF_INET;
  local_addr.sin_port = htons(tunnel->port);
  inet_pton(AF_INET, tunnel->local_ip, &local_addr.sin_addr);

  if (bind(tunnel->udp_sock, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
      LOG_E(GTPU, "Failed to bind UDP socket to %s:%d, error: %s\n",
            tunnel->local_ip, tunnel->port, strerror(errno));
      close(tunnel->udp_sock);
      return -1;
  }

  // --- Step 2: UDP sukses, baru buat TUN ---
  char tun_name[16];
  snprintf(tun_name, sizeof(tun_name), "gtp-tun%d", tunnel_index);
  tunnel->tun_fd = create_tun_device(tun_name);
  if (tunnel->tun_fd < 0) {
      LOG_E(GTPU, "Failed to create TUN device %s\n", tun_name);
      close(tunnel->udp_sock);
      return -1;
  }

  // --- Step 3: Tentukan Subnet IP secara Dinamis ---
  int subnet_base = 50 + tunnel_index;  // Setiap tunnel punya subnet berbeda
  int ip_host = 88;  // Host ID tetap sama pada setiap subnet

  // --- Step 4: Konfigurasi IP pada TUN ---
  char cmd[CMD_BUFFER_SIZE];
  snprintf(cmd, sizeof(cmd), "ip addr add 10.%d.0.%d/24 dev %s", subnet_base, ip_host, tun_name);
  int ret = system(cmd);
  if (ret != 0) {
      LOG_E(GTPU, "Failed to assign IP to %s, code: %d\n", tun_name, ret);
      close(tunnel->udp_sock);
      close(tunnel->tun_fd);
      return -1;
  }

  snprintf(cmd, sizeof(cmd), "ip link set %s up", tun_name);
  ret = system(cmd);
  if (ret != 0) {
      LOG_E(GTPU, "Failed to bring up %s, code: %d\n", tun_name, ret);
      close(tunnel->udp_sock);
      close(tunnel->tun_fd);
      return -1;
  }

  LOG_I(UPEF, "Tunnel %d fully initialized (UDP & TUN ready): %s -> %s (Subnet: 10.%d.0.0/24)\n",
        tunnel_index, tunnel->local_ip, tunnel->remote_ip, subnet_base);

  return 0;
}

int initialize_gtpu_system(upef_t *tunnel) {
  struct sockaddr_in addr = {0};

  tunnel->udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (tunnel->udp_sock < 0) {
      perror("socket creation failed");
      return -1;
  }

  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr(tunnel->local_ip);
  addr.sin_port = htons(tunnel->port);

  if (bind(tunnel->udp_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
      perror("bind failed");
      close(tunnel->udp_sock);
      return -1;
  }

  tunnel->tun_fd = create_tun_device("gtp-tun0");
  if (tunnel->tun_fd < 0) return -1;

  return 0;
}

#ifdef __cplusplus
}
#endif