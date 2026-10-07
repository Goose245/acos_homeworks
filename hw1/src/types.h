#ifndef TYPES_H
#define TYPES_H
#include <stddef.h>
#include<stdbool.h>

typedef enum {
  BARBER_SLEEP,
  BARBER_WORK
} BarberState;

typedef enum {
  CUSTOMER_ARRIVE,
  CUSTOMER_FINISHED
}EventType;

typedef struct {
  int id;
  BarberState state;
  int current_customer_id;
  int total_customers;
}Barber;

typedef struct {
  int id;
  double arrival_time;
  double start_time;
  double finished_time;
}Customer;

typedef struct {
  double time;
  EventType type;
  int customer_id;
  int barber_id;
}Event;

typedef struct {
  int barber_count;
  int waitind_chairs;
  int total_customers;
  double min_arrival_interval;
  double max_arrival_interval;
  double min_haircut_time;
  double max_haircut_time;
  double max_sim_time;
  int delay_ms;
  unsigned int random_seed;
}Config;

typedef struct {
  int total_arrived;
  int total_served;
  int total_out;
  double total_waitind_time;
}Statistics;
#endif