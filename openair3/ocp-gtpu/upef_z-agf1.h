// zyzy
  #ifndef GTP_ITF_H
  #define GTP_ITF_H

  #include <stdint.h>
  #include <stdbool.h>
  #include <pthread.h>

  // Global variable declarations (EXTERN ONLY)
  extern bool gtpu_initialized;

  // GTP Header Structure
  struct gtp_header {
    uint8_t flags;
    uint8_t message_type;
    uint16_t length;
    uint32_t teid;
  };

  typedef struct {
    char local_ip[32];
    char remote_ip[32];
    int port;
    int udp_sock;
    int tun_fd;
    uint32_t teid;
    pthread_mutex_t tunnel_lock;
  } upef_t;

  #define MAX_TUNNELS 1

  // EXTERN DECLARATION ONLY HERE
  extern upef_t gtpu_tunnels[MAX_TUNNELS];

  // Function declarations
  int initialize_gtpu_system();
  int initialize_gtpu_tunnel(upef_t *tunnel, int tunnel_index);
  void forward_to_gtpu(upef_t *tunnel, char *packet, ssize_t len, uint32_t teid);
  void process_gtpu_packet(upef_t *tunnel, char *buffer, ssize_t len);
  void *gtpu_packet_loop_thread(void *arg);

  #endif // GTP_ITF_H