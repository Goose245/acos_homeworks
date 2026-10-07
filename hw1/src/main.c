#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include "types.h"
#include "logger.h"
#include "barbershop.h"

static volatile sig_atomic_t g_top_requested = 0;

static void handle_sigint(int sig) {
  (void)sig;
  g_top_requested = 1;
}

static Config load_default_config(void) {
  Config config ={
    .barber_count = 2,
    .waitind_chairs = 3,
    .total_customers = 10,
    .min_arrival_interval = 1.0,
    .max_arrival_interval = 3.0,
    .min_haircut_time = 2.0,
    .max_haircut_time = 4.0,
    .max_sim_time = 60.0,
    .delay_ms = 50,
    .random_seed = 43
  };
  return config;
}

static void load_config(const char* file,Config* config) {
  FILE* f = fopen(file,"r");
  if (!f) {
    return;
  }
  char line[64];
  double val;
  while (fscanf(f,"%63[^=]= %lf\n",line , &val) == 2) {
    char* token = strtok(line," \t\r\n");
    if (!token || token[0] == '#') {
      continue;
    }
    if (strcmp(token,"barber_count") == 0) config->barber_count = (int)val;
    else if (strcmp(token,"waitind_chairs") == 0) config->waitind_chairs = (int)val;
    else if (strcmp(token,"total_customers") == 0) config->total_customers = (int)val;
    else if (strcmp(token,"min_arrival_interval") == 0) config->min_arrival_interval = val;
    else if (strcmp(token,"max_arrival_interval") == 0) config->max_arrival_interval = val;
    else if (strcmp(token,"min_haircut_time") == 0) config->min_haircut_time = val;
    else if (strcmp(token,"max_haircut_time") == 0) config->max_haircut_time = val;
    else if (strcmp(token,"max_sim_time") == 0) config->max_sim_time = val;
    else if (strcmp(token,"delay_ms") == 0) config->delay_ms = (int)val;
    else if (strcmp(token,"random_seed") == 0) config->random_seed = (unsigned int )val;
  }
  fclose(f);
}

int main(int argc, char* argv[]) {
  signal(SIGINT, handle_sigint);
  Config config = load_default_config();
  const char * config_path = "data/config.cfg";
  const char* log_path = "simulation.log";
  for (int i = 1 ; i < argc ; i++){
    if (strcmp(argv[i],"-c") == 0 &&  i+ 1 < argc) {
      config_path = argv[++i];
    }
    else if (strcmp(argv[i],"-l") == 0 &&  i + 1 < argc) {
      log_path = argv[++i];
    }
  }
  load_config(config_path, &config);
  srand(config.random_seed);
  if (!logger_init(log_path)) {
    perror("logger_init error");
    return 1;
  }
  logger_log(0.0, "Инициализация парикмахерской");
  logger_log(0.0 , "Парикмахеры: %d Стулья: %d Клиенты: %d seed: %d" , config.barber_count, config.waitind_chairs, config.total_customers, config.random_seed);
  BarberShop shop;
  if (!shop_init(&shop, config)) {
    perror("shop_init error");
    logger_close();
    return 1;
  }
  shop_plan_initial_arrival(&shop);
  while (!event_queue_is_empty(&shop.eq) && !g_top_requested) {
    Event event;
    event_queue_pop(&shop.eq, &event);
    shop_handle_event(&shop, event);
    if (config.delay_ms > 0) {
      usleep(config.delay_ms * 1000);
    }
  }
  if (g_top_requested) {
    logger_log(shop.cur_time , "Получен сигнал завершения");
  }
  else {
    logger_log(shop.cur_time , "Все обработано , парикмахерская закрывается");
  }
  shop_print_statistics(&shop);
  shop_destroy(&shop);
  logger_close();
  return 0;
}
