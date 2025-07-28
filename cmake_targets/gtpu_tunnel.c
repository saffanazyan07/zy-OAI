#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <net/if.h>        // Tambahkan ini untuk IFNAMSIZ
#include <linux/if_tun.h>  // Tambahkan ini untuk definisi TUN/TAP
#include <netinet/in.h>

#define GTPU_PORT 2152
#define BUFFER_SIZE 1500

struct gtp_header {
    uint8_t flags;
    uint8_t message_type;
    uint16_t length;
    uint32_t teid;
};

int create_tun_device(char *dev) {
    struct ifreq ifr;
    int fd = open("/dev/net/tun", O_RDWR);
    if (fd < 0) {
        perror("Opening /dev/net/tun failed");
        exit(EXIT_FAILURE);
    }

    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
    strncpy(ifr.ifr_name, dev, IFNAMSIZ);

    if (ioctl(fd, TUNSETIFF, (void *)&ifr) < 0) {
        perror("TUNSETIFF failed");
        close(fd);
        exit(EXIT_FAILURE);
    }
    return fd;
}

void forward_to_gtpu(int udp_sock, struct sockaddr_in *remote_addr, char *packet, ssize_t len, uint32_t teid) {
    char buffer[BUFFER_SIZE];
    struct gtp_header *gtp = (struct gtp_header *)buffer;

    gtp->flags = 0x30;
    gtp->message_type = 0xff;
    gtp->length = htons(len);
    gtp->teid = htonl(teid);

    memcpy(buffer + sizeof(struct gtp_header), packet, len);

    sendto(udp_sock, buffer, sizeof(struct gtp_header) + len, 0,
           (struct sockaddr *)remote_addr, sizeof(*remote_addr));
}

void process_gtpu_packet(char *buffer, ssize_t len, int tun_fd) {
    struct gtp_header *gtp = (struct gtp_header *)buffer;

    if (gtp->message_type == 0xff) {
        write(tun_fd, buffer + sizeof(struct gtp_header), len - sizeof(struct gtp_header));
    }
}

int main() {
    int tun_fd, udp_sock;
    struct sockaddr_in local_addr, remote_addr, sender_addr;
    socklen_t addr_len = sizeof(sender_addr);
    char buffer[BUFFER_SIZE];

    tun_fd = create_tun_device("gtp-tun0");

    udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sock < 0) {
        perror("UDP socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(GTPU_PORT);
    local_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(udp_sock, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
        perror("Bind failed");
        close(udp_sock);
        exit(EXIT_FAILURE);
    }

    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port = htons(GTPU_PORT);
    inet_pton(AF_INET, "192.168.60.99", &remote_addr.sin_addr);  // IP ujung lain

    fd_set read_fds;
    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(tun_fd, &read_fds);
        FD_SET(udp_sock, &read_fds);

        int max_fd = (tun_fd > udp_sock ? tun_fd : udp_sock) + 1;
        select(max_fd, &read_fds, NULL, NULL, NULL);

        if (FD_ISSET(tun_fd, &read_fds)) {
            ssize_t len = read(tun_fd, buffer, BUFFER_SIZE);
            forward_to_gtpu(udp_sock, &remote_addr, buffer, len, 0x12345678);
        }

        if (FD_ISSET(udp_sock, &read_fds)) {
            ssize_t len = recvfrom(udp_sock, buffer, BUFFER_SIZE, 0,
                                   (struct sockaddr *)&sender_addr, &addr_len);
            process_gtpu_packet(buffer, len, tun_fd);
        }
    }

    close(tun_fd);
    close(udp_sock);
    return 0;
}
