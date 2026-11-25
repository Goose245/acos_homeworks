#define _POSIX_C_SOURCE 200809L
#include "net_utils.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

double net_now(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
}

int net_connect(const char* ip, int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(fd);
        return -1;
    }
    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(fd);
        return -1;
    }

    return fd;
}

int net_listen(const char* ip, int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }
    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(fd);
        return -1;
    }
    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(fd);
        return -1;
    }
    if (listen(fd, 64) < 0) {
        perror("listen");
        close(fd);
        return -1;
    }
    return fd;
}

static bool write_all(int fd, const void* buf, size_t len) {
    const uint8_t* ptr = (const uint8_t*)buf;
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = write(fd, ptr + sent, len - sent);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        if (n == 0) {
            return false;
        }
        sent += (size_t)n;
    }
    return true;
}

static bool read_all(int fd, void* buf, size_t len) {
    uint8_t* ptr = (uint8_t*)buf;
    size_t received = 0;
    while (received < len) {
        ssize_t n = read(fd, ptr + received, len - received);
        if (n <= 0) {
            return false;
        }
        received += (size_t)n;
    }
    return true;
}

bool net_send_packet(int fd, const Packet* pkt) {
    Packet wire;
    memset(&wire, 0, sizeof(wire));
    wire.type = (int32_t)htonl((uint32_t)pkt->type);
    wire.id = (int32_t)htonl((uint32_t)pkt->id);
    strncpy(wire.text, pkt->text, MAX_TEXT_LEN - 1);
    return write_all(fd, &wire, sizeof(wire));
}

bool net_recv_packet(int fd, Packet* pkt) {
    Packet wire;
    if (!read_all(fd, &wire, sizeof(wire))) {
        return false;
    }
    pkt->type = (int32_t)ntohl((uint32_t)wire.type);
    pkt->id = (int32_t)ntohl((uint32_t)wire.id);
    memcpy(pkt->text, wire.text, MAX_TEXT_LEN);
    pkt->text[MAX_TEXT_LEN - 1] = '\0';
    return true;
}

void log_stdout(const char* fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    dprintf(STDOUT_FILENO, "%s\n", buf);
}