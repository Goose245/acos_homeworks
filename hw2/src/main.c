#define _POSIX_C_SOURCE 200809L
#include "barbershop.h"
#include "logger.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static volatile sig_atomic_t g_sigint_received = 0;

static void handle_sigint(int sig) {
  (void)sig;
  g_sigint_received = 1;
}
static void load_default_config(Config* config) {
  config->barber_count = 2;
  config->waiting_chairs = 3;
  config->total_customers = 10;
  config->min_arrival_interval = 0.5;
  config->max_arrival_interval = 1.5;
  config->min_haircut_time = 1.0;
  config->max_haircut_time = 2.5;
  config->max_sim_time = 60.0;
  config->delay_ms = 50;
  config->random_seed = 42;
}

static bool load_config(const char* path, Config* config) {
  FILE* f = fopen(path, "r");
  if (!f) {
    return false;
  }
  char key[64];
  double dval;
  int ival;
  while (fscanf(f, "%63s = ", key) ==  1) {
    if (strcmp(key, "barber_count") == 0 && fscanf(f, "%d", &ival) == 1) config->barber_count = ival;
    else if (strcmp(key, "waiting_chairs") == 0 && fscanf(f, "%d", &ival) == 1) config->waiting_chairs = ival;
    else if (strcmp(key, "total_customers") == 0 && fscanf(f, "%d", &ival) == 1) config->total_customers = ival;
    else if (strcmp(key, "min_arrival_interval") == 0 && fscanf(f, "%lf", &dval) == 1) config->min_arrival_interval = dval;
    else if (strcmp(key, "max_arrival_interval") == 0 && fscanf(f, "%lf", &dval) == 1) config->max_arrival_interval = dval;
    else if (strcmp(key, "min_haircut_time") == 0 && fscanf(f, "%lf", &dval) == 1) config->min_haircut_time = dval;
    else if (strcmp(key, "max_haircut_time") == 0 && fscanf(f, "%lf", &dval) == 1) config->max_haircut_time = dval;
    else if (strcmp(key, "max_sim_time") == 0 && fscanf(f, "%lf", &dval) == 1) config->max_sim_time = dval;
    else if (strcmp(key, "delay_ms") == 0 && fscanf(f, "%d", &ival) == 1) config->delay_ms = ival;
    else if (strcmp(key, "random_seed") == 0 && fscanf(f, "%d", &ival) == 1) config->random_seed = ival;
  }
  fclose(f);
  return true;
}

int main(int argc, char* argv[]) {
  Config config;
  load_default_config(&config);
  const char* log_file = NULL;
  for (int i = 1 ;i < argc; i++ ) {
    if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
      load_config(argv[++i], &config);
    }
    else if (strcmp(argv[i], "-l") == 0 && i + 1 < argc) {
      log_file = argv[++i];
    }
  }
  srand(config.random_seed);
  if (!logger_init(log_file)) {
    return 1;
  }
  signal(SIGINT, handle_sigint);
  Barbershop shop;
  if (!shop_init(&shop, &config)) {
    logger_close();
    return 1;
  }
  logger_log("Парметры: Мастеров:%d Стульев:%d Посетителей:%d" , config.barber_count, config.waiting_chairs, config.total_customers);
  shop_start(&shop);
  pthread_t* customer_thread_ = malloc(sizeof(pthread_t) * (size_t)config.total_customers);
  Customer* customers = malloc(sizeof(Customer) * (size_t)config.total_customers);
  int c_customers = 0;
  for (int i = 0; i < config.total_customers; i++) {
    if (g_sigint_received || get_time(&shop) >= config.max_sim_time) {
      logger_log("\nСигнал остановки!");
      break;
    }
    customers[i].id = i + 1;
    customers[i].shop = &shop;
    customers[i].barber_id = -1;
    pthread_create(&customer_thread_[i], NULL, customer_thread, &customers[i]);
    c_customers++;
    double interval = config.min_arrival_interval + ((double)rand() / RAND_MAX) * (config.max_arrival_interval - config.min_arrival_interval);
    usleep((useconds_t)(interval * 100000.0));
  }
  for (int i = 0; i < c_customers; i++) {
    pthread_join(customer_thread_[i], NULL);
  }
  shop_stop(&shop);
  logger_log("Статистика:\n");
  logger_log("Всего посетителей пришло: %d", shop.statistics.total_arrived);
  logger_log("Всего подстрижено : %d", shop.statistics.total_served);
  logger_log("Ушло не подстриженными : %d", shop.statistics.total_left);
  for (int i = 0; i < config.barber_count; i++) {
    logger_log("Парикмахер #%d обслужил: %d клиентов.", shop.barbers[i].id , shop.barbers[i].customer_served);
  }
  free(customer_thread_);
  free(customers);
  shop_destroy(&shop);
  logger_close();
  return 0;
}