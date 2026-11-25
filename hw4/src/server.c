#define _POSIX_C_SOURCE 200809L
#include "net_utils.h"
#include "protocol.h"
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

typedef enum {
    ROLE_NONE = 0,
    ROLE_UNKNOWN = 1,
    ROLE_BARBER = 2,
    ROLE_CUSTOMER = 3,
    ROLE_OBSERVER = 4
} ClientRole;

typedef struct {
    int fd;
    ClientRole role;
    int id;
    bool busy;
    int current_customer_slot;
} ClientSlot;

static volatile sig_atomic_t g_running = 1;
static ClientSlot g_clients[MAX_CLIENTS];
static int g_queue[MAX_CHAIRS];
static int g_queue_head = 0;
static int g_queue_tail = 0;
static int g_queue_count = 0;
static int g_waiting_chairs = 3;
static double g_start_time = 0.0;

static int g_total_arrived = 0;
static int g_total_served = 0;
static int g_total_left = 0;

static void handle_sigint(int sig) {
    (void)sig;
    g_running = 0;
}

static double elapsed_time(void) {
    return net_now() - g_start_time;
}

static void init_slots(void) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        g_clients[i].fd = -1;
        g_clients[i].role = ROLE_NONE;
        g_clients[i].id = 0;
        g_clients[i].busy = false;
        g_clients[i].current_customer_slot = -1;
    }
}

static void close_slot(int idx) {
    if (idx < 0 || idx >= MAX_CLIENTS || g_clients[idx].fd < 0) {
        return;
    }
    close(g_clients[idx].fd);
    g_clients[idx].fd = -1;
    g_clients[idx].role = ROLE_NONE;
    g_clients[idx].id = 0;
    g_clients[idx].busy = false;
    g_clients[idx].current_customer_slot = -1;
}

static void broadcast_event(const char* fmt, ...) {
    char body[MAX_TEXT_LEN - 32];
    va_list args;
    va_start(args, fmt);
    vsnprintf(body, sizeof(body), fmt, args);
    va_end(args);

    Packet pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.type = MSG_OBSERVER_EVENT;
    pkt.id = 0;
    snprintf(pkt.text, sizeof(pkt.text), "[Время %6.2lfs] %s", elapsed_time(), body);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_clients[i].fd >= 0 && g_clients[i].role == ROLE_OBSERVER) {
            if (!net_send_packet(g_clients[i].fd, &pkt)) {
                log_stdout("[Сервер] Наблюдатель отключился (fd=%d)", g_clients[i].fd);
                close_slot(i);
            }
        }
    }
}

static int find_free_barber(void) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_clients[i].fd >= 0 && g_clients[i].role == ROLE_BARBER && !g_clients[i].busy) {
            return i;
        }
    }
    return -1;
}

static void try_assign_barber(void) {
    while (g_queue_count > 0) {
        int barber_idx = find_free_barber();
        if (barber_idx < 0) {
            return;
        }
        int cust_idx = g_queue[g_queue_head];
        g_queue_head = (g_queue_head + 1) % MAX_CHAIRS;
        g_queue_count--;
        if (cust_idx < 0 || cust_idx >= MAX_CLIENTS || g_clients[cust_idx].fd < 0) {
            continue;
        }
        g_clients[barber_idx].busy = true;
        g_clients[barber_idx].current_customer_slot = cust_idx;
        Packet pkt;
        memset(&pkt, 0, sizeof(pkt));
        pkt.type = MSG_ASSIGN_CUSTOMER;
        pkt.id = g_clients[cust_idx].id;
        if (!net_send_packet(g_clients[barber_idx].fd, &pkt)) {
            close_slot(barber_idx);
            continue;
        }
        log_stdout("[Сервер] Назначил клиента #%d парикмахеру #%d", g_clients[cust_idx].id, g_clients[barber_idx].id);
        broadcast_event("Парикмахер #%d проснулся и пригласил клиента #%d (в очереди: %d/%d)",
                        g_clients[barber_idx].id, g_clients[cust_idx].id, g_queue_count, g_waiting_chairs);
    }
}

static void handle_packet(int idx, const Packet* pkt) {
    if (pkt->type == MSG_HELLO_BARBER) {
        g_clients[idx].role = ROLE_BARBER;
        g_clients[idx].id = pkt->id;
        g_clients[idx].busy = false;
        g_clients[idx].current_customer_slot = -1;
        log_stdout("[Сервер] Подключился парикмахер #%d", pkt->id);
        broadcast_event("Парикмахер #%d уснул в ожидании клиента...", pkt->id);
        try_assign_barber();
    } else if (pkt->type == MSG_HELLO_CUSTOMER) {
        g_clients[idx].role = ROLE_CUSTOMER;
        g_clients[idx].id = pkt->id;
        g_total_arrived++;
        log_stdout("[Сервер] Пришел клиент #%d", pkt->id);
        broadcast_event("Клиент #%d вошел в парикмахерскую.", pkt->id);
        if (g_queue_count < g_waiting_chairs) {
            g_queue[g_queue_tail] = idx;
            g_queue_tail = (g_queue_tail + 1) % MAX_CHAIRS;
            g_queue_count++;
            Packet reply;
            memset(&reply, 0, sizeof(reply));
            reply.type = MSG_CUSTOMER_WAIT;
            reply.id = pkt->id;
            net_send_packet(g_clients[idx].fd, &reply);
            broadcast_event("Клиент #%d занял стул в очереди (очередь: %d/%d).",
                            pkt->id, g_queue_count, g_waiting_chairs);
            try_assign_barber();
        } else {
            g_total_left++;
            Packet reply;
            memset(&reply, 0, sizeof(reply));
            reply.type = MSG_CUSTOMER_LEFT;
            reply.id = pkt->id;
            net_send_packet(g_clients[idx].fd, &reply);
            log_stdout("[Сервер] Клиент #%d ушел: нет свободных стульев", pkt->id);
            broadcast_event("Клиент #%d ушел необслуженным (все стулья заняты).", pkt->id);
            close_slot(idx);
        }
    } else if (pkt->type == MSG_HELLO_OBSERVER) {
        g_clients[idx].role = ROLE_OBSERVER;
        g_clients[idx].id = pkt->id;
        log_stdout("[Сервер] Подключился наблюдатель (fd=%d)", g_clients[idx].fd);
        Packet info;
        memset(&info, 0, sizeof(info));
        info.type = MSG_OBSERVER_EVENT;
        snprintf(info.text, sizeof(info.text),
                 "[Время %6.2lfs] Подключение к парикмахерской успешно. В очереди: %d/%d, обслужено: %d, ушло: %d.",
                 elapsed_time(), g_queue_count, g_waiting_chairs, g_total_served, g_total_left);
        if (!net_send_packet(g_clients[idx].fd, &info)) {
            close_slot(idx);
        }
    } else if (pkt->type == MSG_HAIRCUT_DONE && g_clients[idx].role == ROLE_BARBER) {
        int cust_idx = g_clients[idx].current_customer_slot;
        int cust_id = pkt->id;

        g_total_served++;
        log_stdout("[Сервер] Парикмахер #%d завершил стрижку клиента #%d", g_clients[idx].id, cust_id);
        broadcast_event("Парикмахер #%d завершил стрижку клиента #%d.", g_clients[idx].id, cust_id);

        if (cust_idx >= 0 && cust_idx < MAX_CLIENTS && g_clients[cust_idx].fd >= 0) {
            Packet reply;
            memset(&reply, 0, sizeof(reply));
            reply.type = MSG_CUSTOMER_SERVED;
            reply.id = cust_id;
            net_send_packet(g_clients[cust_idx].fd, &reply);
            close_slot(cust_idx);
        }
        broadcast_event("Клиент #%d покинул парикмахерскую подстриженным.", cust_id);

        g_clients[idx].busy = false;
        g_clients[idx].current_customer_slot = -1;
        try_assign_barber();

        if (!g_clients[idx].busy) {
            broadcast_event("Парикмахер #%d уснул в ожидании клиента...", g_clients[idx].id);
        }
    }
}

int main(int argc, char** argv) {
    const char* ip = "127.0.0.1";
    int port = 8080;
    int opt;
    while ((opt = getopt(argc, argv, "a:p:w:")) != -1) {
        if (opt == 'a') {
            ip = optarg;
        } else if (opt == 'p') {
            port = atoi(optarg);
        } else if (opt == 'w') {
            g_waiting_chairs = atoi(optarg);
        }
    }
    if (g_waiting_chairs < 1 || g_waiting_chairs > MAX_CHAIRS) {
        g_waiting_chairs = 3;
    }
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);
    int listen_fd = net_listen(ip, port);
    if (listen_fd < 0) {
        return 1;
    }
    init_slots();
    g_start_time = net_now();
    log_stdout("[Сервер] Запущен на %s:%d (стульев ожидания: %d)", ip, port, g_waiting_chairs);
    struct pollfd fds[MAX_CLIENTS + 1];
    while (g_running) {
        fds[0].fd = listen_fd;
        fds[0].events = POLLIN;
        fds[0].revents = 0;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            fds[i + 1].fd = g_clients[i].fd;
            fds[i + 1].events = POLLIN;
            fds[i + 1].revents = 0;
        }
        int ready = poll(fds, MAX_CLIENTS + 1, 200);
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("poll");
            break;
        }
        if (ready == 0) {
            continue;
        }
        if (fds[0].revents & POLLIN) {
            int client_fd = accept(listen_fd, NULL, NULL);
            if (client_fd >= 0) {
                int slot = -1;
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (g_clients[i].fd < 0) {
                        slot = i;
                        break;
                    }
                }
                if (slot >= 0) {
                    g_clients[slot].fd = client_fd;
                    g_clients[slot].role = ROLE_UNKNOWN;
                } else {
                    close(client_fd);
                }
            }
        }
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (g_clients[i].fd < 0) {
                continue;
            }
            short rev = fds[i + 1].revents;
            if (rev & (POLLHUP | POLLERR | POLLNVAL)) {
                if (g_clients[i].role == ROLE_OBSERVER) {
                    log_stdout("[Сервер] Наблюдатель отключился (fd=%d)", g_clients[i].fd);
                }
                close_slot(i);
                continue;
            }
            if (rev & POLLIN) {
                Packet pkt;
                if (!net_recv_packet(g_clients[i].fd, &pkt)) {
                    if (g_clients[i].role == ROLE_OBSERVER) {
                        log_stdout("[Сервер] Наблюдатель отключился (fd=%d)", g_clients[i].fd);
                    }
                    close_slot(i);
                } else {
                    handle_packet(i, &pkt);
                }
            }
        }
    }

    log_stdout("[Сервер] Остановка. Итоги: пришло=%d, обслужено=%d, ушло=%d",
               g_total_arrived, g_total_served, g_total_left);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        close_slot(i);
    }
    close(listen_fd);
    return 0;
}