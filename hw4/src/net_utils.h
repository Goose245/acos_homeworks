#ifndef NET_UTILS_H
#define NET_UTILS_H

#include "protocol.h"
#include <stdbool.h>

double net_now(void);
int net_connect(const char* ip, int port);
int net_listen(const char* ip, int port);
bool net_send_packet(int fd, const Packet* pkt);
bool net_recv_packet(int fd, Packet* pkt);
void log_stdout(const char* fmt, ...);

#endif