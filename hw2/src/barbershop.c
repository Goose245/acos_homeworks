#define _POSIX_C_SOURCE 200809L
#include "barbershop.h"
#include "logger.h"
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

static double get_current_time(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
}

double get_time(const Barbershop* shop) {
  return get_current_time()- shop->start_time;
}

static sem_t* create_semaphore(const char* name_pref,int value) {
  char name[64];
  snprintf(name,sizeof(name),"/sem_%s_%d_%ld",name_pref,getpid(), random());
  sem_unlink(name);
  sem_t* sem = sem_open(name,O_CREAT | O_EXCL,0644,value);
  if (sem != SEM_FAILED) {
    sem_unlink(name);
  }
  return sem;
}

bool shop_init(Barbershop* shop, const Config* config) {
  shop->config = *config;
  shop->statistics = (Statistics){0,0,0,0.0};
  shop->waiting_count = 0;
  shop->queue_head = 0;
  shop->queue_tail = 0;
  shop->stop_requested = false;
  shop->start_time = get_current_time();
  if (pthread_mutex_init(&shop->mutex,NULL) != 0) {
    return false;
  }
  shop->sem_customers = create_semaphore("cust",0);
  if (shop->sem_customers == SEM_FAILED) {
    pthread_mutex_destroy(&shop->mutex);
    return false;
  }
  for (int i = 0 ; i < config->barber_count; i++) {
    shop->barbers[i].id = i + 1;
    shop->barbers[i].state = BARBER_SLEEP;
    shop->barbers[i].current_customer_id = -1;
    shop->barbers[i].customer_served = 0;
    shop->barbers[i].shop = shop;
  }
  return true;
}
bool shop_start(Barbershop* shop) {
  for (int i = 0 ; i < shop->config.barber_count; i++) {
    if (pthread_create(&shop->barbers[i].thread,NULL,barbershop_thread,&shop->barbers[i]) != 0 ) {
      return false;
    }
  }
  return true;
}
void *barbershop_thread(void *arg) {
  Barber* b = (Barber*)arg;
  Barbershop* shop = b->shop;
  while (true) {
    pthread_mutex_lock(&shop->mutex);
    if (shop->waiting_count == 0) {
      b->state = BARBER_SLEEP;
      b->current_customer_id = -1;
      logger_log("[Время %6.2fs] Парикмахер %d: уснул в ожидании клиентов.", get_time(shop), b->id);
    }
    pthread_mutex_unlock(&shop->mutex);
    sem_wait(shop->sem_customers);
    pthread_mutex_lock(&shop->mutex);
    if (shop->stop_requested && shop->waiting_count == 0) {
      pthread_mutex_unlock(&shop->mutex);
      break;
    }
    assert(shop->waiting_count > 0);
    Customer* customer = shop->waiting_queue[shop->queue_head];
    shop->queue_head = (shop->queue_head + 1) % MAX_CHAIRS;
    shop->waiting_count--;
    b->state = BARBER_WORK;
    b->current_customer_id = customer->id;
    customer->barber_id = b->id;
    customer->start_time = get_time(shop);
    logger_log("[Время %6.2fs] Парикмахер #%d проснулся и пригласил клиента #%d(в осереди осталось: %d).", customer->start_time,b->id ,customer->id,shop->waiting_count );
    pthread_mutex_unlock(&shop->mutex);
    double duration = shop->config.min_haircut_time + ((double)rand() / RAND_MAX) * (shop->config.max_haircut_time - shop->config.min_haircut_time);
    usleep((useconds_t)(duration * 100000.0));
    pthread_mutex_lock(&shop->mutex);
    customer->finished_time = get_time(shop);
    b->customer_served++;
    shop->statistics.total_served++;
    shop->statistics.total_wait_time += (customer->start_time - customer->arrival_time);
    logger_log("[Время %6.2fs] Парикмахер #%d завершил стрижку клиента #%d", customer->finished_time , b->id, customer->id);
    pthread_mutex_unlock(&shop->mutex);
    sem_post(customer->sem_service_done);
  }
  return NULL;
}

void *customer_thread(void *arg) {
  Customer* customer = (Customer*)arg;
  Barbershop* shop = customer->shop;
  customer->barber_id = -1;
  customer->arrival_time = get_time(shop);
  pthread_mutex_lock(&shop->mutex);
  shop->statistics.total_arrived++;
  logger_log("[Время %6.2fs] Клиент #%d вошел.", customer->arrival_time , customer->id);
  if (shop->waiting_count < shop->config.waiting_chairs) {
    shop->waiting_queue[shop->queue_tail] = customer;
    shop->queue_tail = (shop->queue_tail + 1) % MAX_CHAIRS;
    shop->waiting_count++;
    logger_log("[Время %6.2fs]  Клиент #%d занял стул ожидания ( всего в очереди: %d/%d).", customer->arrival_time, customer->id,shop->waiting_count,shop->config.waiting_chairs);
    customer->sem_service_done = create_semaphore("done",0);
    sem_post(shop->sem_customers);
    pthread_mutex_unlock(&shop->mutex);
    sem_wait(customer->sem_service_done);
    sem_close(customer->sem_service_done);
    logger_log("[Время %6.2fs] Клиент #%d покинул парикмахерскую подстриженным.",get_time(shop), customer->id );
  }
  else {
    shop->statistics.total_left++;
    logger_log("[Время %6.2fs] Клиент #%d ушел необслуженным(нет стульев).", customer->arrival_time , customer->id);
    pthread_mutex_unlock(&shop->mutex);
  }
  return NULL;
}

void shop_stop(Barbershop* shop) {
  pthread_mutex_lock(&shop->mutex);
  shop->stop_requested = true;
  for (int i = 0 ; i < shop->config.barber_count; i++) {
    sem_post(shop->sem_customers);
  }
  pthread_mutex_unlock(&shop->mutex);
  for (int i = 0 ; i < shop->config.barber_count; i++) {
    pthread_join(shop->barbers[i].thread, NULL);
  }
}

void shop_destroy(Barbershop* shop) {
  sem_close(shop->sem_customers);
  pthread_mutex_destroy(&shop->mutex);
}