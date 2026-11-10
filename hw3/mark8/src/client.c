#define _POSIX_C_SOURCE 200809L
#include "barbershop.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include  <sys/wait.h>

int main(int argc, char *argv[]) {
  int total_customers = 10;
  double min_interval = 0.5;
  double max_interval = 1.5;
  for (int i = 1 ; i < argc ; i++) {
    if (strcmp(argv[i], "-n") == 0 && i +  1 < argc) {
      total_customers  = atoi(argv[++i]);
    }
  }
  if (!logger_init(NULL,"/barbershop_log_sem")) {
    return 1;
  }
  if (!shop_attach()) {
    logger_log("Не удалось подключиться к серверу.");
    logger_close();
    return 1;
  }
  logger_log("Генерация клиентов началась. Отправлеяется %d посетителей.", total_customers);
  srand(getpid());
  pid_t* customer_pids = malloc(sizeof(pid_t) * (size_t)total_customers);
  int c_customers = 0;
  for (int i = 0 ; i < total_customers ; i++) {
    pid_t pid = fork();
    if (pid< 0) {
      perror("fork failed");
      break;
    }
    else if (pid == 0) {
      Customer customer;
      customer.id = (getpid() % 1000) * 100+ i + 1;
      customer.barber_id = -1;
      customer_process(&customer);
    }
    else {
      customer_pids[c_customers++] = pid;
    }
    double interval = min_interval + ((double)rand() / RAND_MAX) * (max_interval - min_interval);
    usleep((useconds_t)(interval * 1000000.0));
  }
  for (int i = 0 ; i< c_customers ; i++) {
    waitpid(customer_pids[i], NULL, 0);
  }
  free(customer_pids);
  logger_log("Все клиенты этой группы прошли.");
  logger_close();
  return 0;
}