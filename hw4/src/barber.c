#define _POSIX_C_SOURCE 200809L
#include "net_utils.h"
#include "protocol.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t g_running = 1;

static void handle_sigint(int sig) {
    (void)sig;
    g_running = 0;
}

static void sleep_seconds(double sec) {
    if (sec <= 0.0) {
        return;
    }
    struct timespec ts;
    ts.tv_sec = (time_t)sec;
    ts.tv_nsec = (long)((sec - (double)ts.tv_sec) * 1000000000.0);
    nanosleep(&ts, NULL);
}

int main(int argc, char** argv) {
    const char* ip = "127.0.0.1";
    int port = 8080;
    int barber_id = 1;
    double min_time = 1.0;
    double max_time = 3.0;
    int opt;
    while ((opt = getopt(argc, argv, "a:p:i:m:M:")) != -1) {
        if (opt == 'a') {
            ip = optarg;
        } else if (opt == 'p') {
            port = atoi(optarg);
        } else if (opt == 'i') {
            barber_id = atoi(optarg);
        } else if (opt == 'm') {
            min_time = atof(optarg);
        } else if (opt == 'M') {
            max_time = atof(optarg);
        }
    }
    signal(SIGPIPE, SIG_IGN);
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigint;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    srand((unsigned int)(time(NULL) ^ getpid()));
    int fd = net_connect(ip, port);
    if (fd < 0) {
        return 1;
    }
    Packet hello;
    memset(&hello, 0, sizeof(hello));
    hello.type = MSG_HELLO_BARBER;
    hello.id = barber_id;
    if (!net_send_packet(fd, &hello)) {
        close(fd);
        return 1;
    }
    log_stdout("[Парикмахер #%d] Подключился к %s:%d, ожидаю клиентов...", barber_id, ip, port);
    while (g_running) {
        Packet pkt;
        if (!net_recv_packet(fd, &pkt)) {
            break;
        }
        if (pkt.type == MSG_ASSIGN_CUSTOMER) {
            int cust_id = pkt.id;
            double dur = min_time + ((double)rand() / (double)RAND_MAX) * (max_time - min_time);
            log_stdout("[Парикмахер #%d] Начал стрижку клиента #%d (%.2f сек)", barber_id, cust_id, dur);
            sleep_seconds(dur);
            if (!g_running) {
                break;
            }
            log_stdout("[Парикмахер #%d] Завершил стрижку клиента #%d", barber_id, cust_id);
            Packet done;
            memset(&done, 0, sizeof(done));
            done.type = MSG_HAIRCUT_DONE;
            done.id = cust_id;
            if (!net_send_packet(fd, &done)) {
                break;
            }
        }
    }
    log_stdout("[Парикмахер #%d] Завершил работу.", barber_id);
    close(fd);
    return 0;
}