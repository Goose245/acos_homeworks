#define _POSIX_C_SOURCE 200809L
#include "barbershop.h"
#include "logger.h"
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <semaphore.h>

static BarbershopShared* g_shop = NULL;


static double get_current_time(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
}

double get_time(void) {
  if (!g_shop) {
    return 0.0;
  }
  return get_current_time() - g_shop->start_time;
}
bool shop_init(const Config* config) {
  shm_unlink(SHM_NAME);
  int shm_fd = shm_open(SHM_NAME,O_CREAT |  O_RDWR ,0666);
  if (shm_fd < 0 ) {
    perror("shim open fail");
    return false;
  }
  if (ftruncate(shm_fd, sizeof(BarbershopShared)) == -1 ) {
    perror("ftruncate fail");
    return false;
  }
  g_shop = mmap(0, sizeof(BarbershopShared), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
  close(shm_fd);
  if (g_shop == MAP_FAILED) {
    perror("mmap fail");
    return false;
  }
  g_shop->config = *config;
  g_shop->statistics = (Statistics){0,0,0,0.0};
  g_shop->waiting_count = 0;
  g_shop->queue_head = 0;
  g_shop->queue_tail = 0;
  g_shop->stop_requested = false;
  g_shop->start_time = get_current_time();
  for (int i = 0; i < config->barber_count; i++) {
    g_shop->barbers[i].id = i + 1;
    g_shop->barbers[i].state = BARBER_SLEEP;
    g_shop->barbers[i].current_customer_id = -1;
    g_shop->barbers[i].customer_served = 0;
    g_shop->barbers[i].pid = -1;
  }
  if (sem_init(&g_shop->mutex, 1, 1) != 0) {
    perror("sem_init fail");
    return false;
  }
  if (sem_init(&g_shop->customer_sem, 1 , 0) != 0) {
    perror("sem_init  customer fail");
    return false;
  }
  return true;
}

void barber_process(Barber* barber) {
  srand(getpid());
  while (true) {
    sem_wait(&g_shop->mutex);
    if (g_shop->waiting_count == 0) {
      barber->state = BARBER_SLEEP;
      barber->current_customer_id = -1;
      logger_log("[Время %6.2lfs] Парикмахер #%d: уснул в ожидании клиента..." , get_time() , barber->id);
    }
    sem_post(&g_shop->mutex);
    sem_wait(&g_shop->customer_sem);
    sem_wait(&g_shop->mutex);
    if (g_shop->stop_requested && g_shop->waiting_count ==  0 ) {
      sem_post(&g_shop->mutex);
      break;
    }
    assert(g_shop->waiting_count > 0);
    int cust_idx = g_shop->queue_head;
    Customer* customer = &g_shop->waiting_queue[cust_idx];
    g_shop->queue_head = (g_shop->queue_head + 1) % MAX_CHAIRS;
    g_shop->waiting_count--;
    barber->state = BARBER_WORK;
    barber->current_customer_id = customer->id;
    customer->barber_id = barber->id;
    customer->start_time = get_time();
    logger_log("[Время %6.2lfs] Парикмахер #%d проснулся и пригласил клиента #%d.( в очереди : %d)" , customer->start_time , barber->id , customer->id, g_shop->waiting_count);
    sem_post(&g_shop->mutex);
    double duration = g_shop->config.min_haircut_time + ((double) rand() / RAND_MAX) * (g_shop->config.max_haircut_time- g_shop->config.min_haircut_time);
    usleep((useconds_t)(duration * 1000000.0));
    sem_wait(&g_shop->mutex);
    customer->finish_time = get_time();
    barber->customer_served++;
    g_shop->statistics.total_served++;
    g_shop->statistics.total_wait_time += (customer->start_time - customer->arrival_time);
    logger_log("[Время %6.2lfs] Парикмахер #%d завершил стрижку клиента #%d" , customer->finish_time , barber->id , customer->id);
    sem_post(&customer->sem);
    sem_post(&g_shop->mutex);
  }
  exit(0);
}

bool shop_start(void) {
  for (int i = 0 ; i < g_shop->config.barber_count; i++) {
    pid_t pid = fork();
    if (pid < 0) {
      perror("fork fail");
      return false;
    }
    else if (pid == 0) {
      barber_process(&g_shop->barbers[i]);
    }
    else {
      g_shop->barbers[i].pid = pid;
    }
  }
  return true;
}

void customer_process(Customer* customer) {
  srand(getpid());
  customer->arrival_time = get_time();
  sem_wait(&g_shop->mutex);
  g_shop->statistics.total_arrived++;
  logger_log("[Время %6.2lfs] Клиент #%d вошел." , customer->arrival_time , customer->id);
  if (g_shop->waiting_count < g_shop->config.waiting_chairs) {
    int tail_idx = g_shop->queue_tail;
    g_shop->waiting_queue[tail_idx] = *customer;
    Customer* queued_customer = &g_shop->waiting_queue[tail_idx];
    if (sem_init(&queued_customer->sem, 1 , 0) != 0) {
      perror("sem_init fail");
      exit(1);
    }
    g_shop->queue_tail = (g_shop->queue_tail + 1) % MAX_CHAIRS;
    g_shop->waiting_count++;
    logger_log("[Время %6.2lfs] Клиент #%d занял стул в очереди.( очередь %d/%d) " , customer->arrival_time , customer->id, g_shop->waiting_count, g_shop->config.waiting_chairs);
    sem_post(&g_shop->customer_sem);
    sem_post(&g_shop->mutex);
    sem_wait(&queued_customer->sem);
    sem_destroy(&queued_customer->sem);
    logger_log("[Время %6.2lfs] Клиент #%d покинул парикмахерскую подстриженным." , get_time() , customer->id);
  }
  else {
    g_shop->statistics.total_left++;
    logger_log("[Время %6.2lfs] Клиент #%d ушел не обслуженным." , customer->arrival_time , customer->id);
    sem_post(&g_shop->mutex);
  }

  exit(0);
}

void shop_stop(void) {
  sem_wait(&g_shop->mutex);
  g_shop->stop_requested = true;
  for (int i = 0 ; i < g_shop->config.barber_count; i++) {
    sem_post(&g_shop->customer_sem);
  }
  sem_post(&g_shop->mutex);
  for (int i = 0 ; i < g_shop->config.barber_count; i++) {
    if (g_shop->barbers[i].pid > 0) {
      waitpid(g_shop->barbers[i].pid, NULL, 0);
    }
  }
}

void shop_destroy(void) {
  if (g_shop != MAP_FAILED && g_shop != NULL) {
    sem_destroy(&g_shop->mutex);
    sem_destroy(&g_shop->customer_sem);
    munmap(g_shop,sizeof(BarbershopShared));
  }
  shm_unlink(SHM_NAME);
}

bool shop_attach(void) {
  int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
  if (shm_fd < 0) {
    perror("shm_open fail");
    return false;
  }
  g_shop = mmap(0, sizeof(BarbershopShared), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
  close(shm_fd);
  if (g_shop == MAP_FAILED) {
    perror("mmap fail");
    return false;
  }
  return true;
}

void shop_print_stats(void) {
  if (!g_shop) {
    return;
  }
  logger_log("Всего посетителей пришло %d", g_shop->statistics.total_arrived);
  logger_log("Всего подстрижено %d", g_shop->statistics.total_served);
  logger_log("Всего ушло без подстрижки %d", g_shop->statistics.total_left);
  for (int i = 0 ; i < g_shop->config.barber_count; i++) {
    logger_log("Парикмахер #%d обслужил - %d клиентов", g_shop->barbers[i].id, g_shop->barbers[i].customer_served);
  }
}