#ifndef  TYPES_H
#define  TYPES_H

#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <semaphore.h>

#define MAX_BARBERS 64
#define MAX_CHAIRS 128

#define SHM_NAME "/barbershop_shm"


typedef struct {
  int barber_count;
  int waiting_chairs;
  int total_customers;
  double min_arrival_interval;
  double max_arrival_interval;
  double min_haircut_time;
  double max_haircut_time;
  double max_sim_time;
  int delay_ms;
  unsigned int random_seed;
} Config;

typedef struct {
  int total_arrived;
  int total_served;
  int total_left;
  double total_wait_time;
} Statistics;

typedef enum {
  BARBER_SLEEP,
  BARBER_WORK
}BarberState;

typedef struct {
  int id;
  double arrival_time;
  double start_time;
  double finish_time;
  int barber_id;
  sem_t sem;
} Customer;

typedef struct {
  int id;
  BarberState state;
  int current_customer_id;
  int customer_served;
  pid_t pid;
} Barber;

typedef struct {
  Config config;
  Statistics statistics;
  Barber barbers[MAX_BARBERS];
  Customer waiting_queue[MAX_CHAIRS];
  int waiting_count;
  int queue_head;
  int queue_tail;
  volatile bool stop_requested;
  double start_time;
  sem_t mutex;
  sem_t customer_sem;
}BarbershopShared;
#endif