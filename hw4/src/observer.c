#define _POSIX_C_SOURCE 200809L
#include "net_utils.h"
#include "protocol.h"
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static volatile sig_atomic_t g_running = 1;

static void handle_sigint(int sig) {
  (void)sig;
  g_running = 0;
}

int main(int argc, char** argv) {
  const char* ip = "127.0.0.1";
  int port = 8080;
  const char* log_path = NULL;
  int opt;
  while ((opt = getopt(argc, argv, "a:p:l:")) != -1) {
    if (opt == 'a') {
      ip = optarg;
    } else if (opt == 'p') {
      port = atoi(optarg);
    } else if (opt == 'l') {
      log_path = optarg;
    }
  }
  signal(SIGPIPE, SIG_IGN);
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = handle_sigint;
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);
  int log_fd = -1;
  if (log_path != NULL) {
    log_fd = open(log_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (log_fd < 0) {
      perror("open");
      return 1;
    }
  }
  int fd = net_connect(ip, port);
  if (fd < 0) {
    if (log_fd >= 0) {
      close(log_fd);
    }
    return 1;
  }
  Packet hello;
  memset(&hello, 0, sizeof(hello));
  hello.type = MSG_HELLO_OBSERVER;
  hello.id = getpid();
  if (!net_send_packet(fd, &hello)) {
    close(fd);
    if (log_fd >= 0) {
      close(log_fd);
    }
    return 1;
  }
  log_stdout("[Наблюдатель] Подключен к %s:%d", ip, port);
  while (g_running) {
    Packet pkt;
    if (!net_recv_packet(fd, &pkt)) {
      break;
    }
    if (pkt.type == MSG_OBSERVER_EVENT) {
      dprintf(STDOUT_FILENO, "%s\n", pkt.text);
      if (log_fd >= 0) {
        dprintf(log_fd, "%s\n", pkt.text);
      }
    }
  }
  log_stdout("[Наблюдатель] Отключен.");
  close(fd);
  if (log_fd >= 0) {
    close(log_fd);
  }
  return 0;
}