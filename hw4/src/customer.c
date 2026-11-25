#define _POSIX_C_SOURCE 200809L
#include "net_utils.h"
#include "protocol.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static void sleep_seconds(double sec) {
    if (sec <= 0.0) {
        return;
    }
    struct timespec ts;
    ts.tv_sec = (time_t)sec;
    ts.tv_nsec = (long)((sec - (double)ts.tv_sec) * 1000000000.0);
    nanosleep(&ts, NULL);
}

static void run_customer(const char* ip, int port, int cust_id) {
    int fd = net_connect(ip, port);
    if (fd < 0) {
        exit(1);
    }
    Packet hello;
    memset(&hello, 0, sizeof(hello));
    hello.type = MSG_HELLO_CUSTOMER;
    hello.id = cust_id;
    if (!net_send_packet(fd, &hello)) {
        close(fd);
        exit(1);
    }
    log_stdout("[Клиент #%d] Вошел в парикмахерскую.", cust_id);
    Packet reply;
    while (net_recv_packet(fd, &reply)) {
        if (reply.type == MSG_CUSTOMER_WAIT) {
            log_stdout("[Клиент #%d] Занял стул в очереди.", cust_id);
        } else if (reply.type == MSG_CUSTOMER_SERVED) {
            log_stdout("[Клиент #%d] Подстрижен и уходит.", cust_id);
            break;
        } else if (reply.type == MSG_CUSTOMER_LEFT) {
            log_stdout("[Клиент #%d] Мест нет, уходит необслуженным.", cust_id);
            break;
        }
    }
    close(fd);
    exit(0);
}

int main(int argc, char** argv) {
    const char* ip = "127.0.0.1";
    int port = 8080;
    int count = 10;
    int start_id = 1;
    double min_interval = 0.5;
    double max_interval = 1.5;
    int opt;
    while ((opt = getopt(argc, argv, "a:p:n:s:m:M:")) != -1) {
        if (opt == 'a') {
            ip = optarg;
        } else if (opt == 'p') {
            port = atoi(optarg);
        } else if (opt == 'n') {
            count = atoi(optarg);
        } else if (opt == 's') {
            start_id = atoi(optarg);
        } else if (opt == 'm') {
            min_interval = atof(optarg);
        } else if (opt == 'M') {
            max_interval = atof(optarg);
        }
    }
    signal(SIGPIPE, SIG_IGN);
    srand((unsigned int)(time(NULL) ^ getpid()));
    log_stdout("[Генератор] Отправка %d клиентов на %s:%d", count, ip, port);
    for (int i = 0; i < count; i++) {
        double delay = min_interval + ((double)rand() / (double)RAND_MAX) * (max_interval - min_interval);
        sleep_seconds(delay);

        pid_t pid = fork();
        if (pid == 0) {
            run_customer(ip, port, start_id + i);
        }
    }
    for (int i = 0; i < count; i++) {
        wait(NULL);
    }
    log_stdout("[Генератор] Все клиенты завершили работу.");
    return 0;
}