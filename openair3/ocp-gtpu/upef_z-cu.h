//==============================================================================//
//-------------------------- GTP Interface Header ------------------------------//
//----------------------------- edited by zyzy ---------------------------------//
//==============================================================================//

#ifndef GTPU_UPEF_H
#define GTPU_UPEF_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef IFF_TUN
#define IFF_TUN 0x0001
#endif

#ifndef IFF_NO_PI
#define IFF_NO_PI 0x1000
#endif

#ifndef TUNSETIFF
#define TUNSETIFF _IOW('T', 202, int)
#endif

#define MAX_TUNNELS 2
#define BUFFER_SIZE 65536
#define CMD_BUFFER_SIZE 256

// Struktur GTP-U Tunnel
typedef struct {
  char local_ip[32];
  char remote_ip[32];
  int port;
  int udp_sock;
  int tun_fd;
  uint32_t teid;
  pthread_mutex_t tunnel_lock;
} upef_t;

/* ===== Global variables ===== */
extern upef_t gtpu_tunnels[MAX_TUNNELS];
extern pthread_mutex_t gtp_lock;
extern bool gtpu_initialized;

/* ===== API ===== */
int initialize_gtpu_system(upef_t *tunnel);
int initialize_gtpu_tunnel(upef_t *tunnel, int tunnel_index);
int create_tun_device(const char *dev);
void forward_to_gtpu(upef_t *tunnel, char *data, ssize_t len, uint32_t teid);
void process_gtpu_packet(upef_t *tunnel, char *buffer, ssize_t len);
void *gtpu_packet_loop_thread(void *arg);
void start_gtpu_threads();

#ifdef __cplusplus
}
#endif

#endif // GTP_ITF_H
