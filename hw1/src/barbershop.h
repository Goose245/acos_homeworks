#ifndef BARBERSHOP_BARBERHOP_H
#define BARBERSHOP_BARBERHOP_H
#include "types.h"
#include "event_queue.h"

typedef struct {
  Config config;
  Barber* barbers;
  Customer* waiting_chairs;
  int waiting_count;
  Statistics statistics;
  EventQueue eq;
  double cur_time;
  int next_customer_id;
}BarberShop;

bool shop_init(BarberShop* shop, Config config);
void shop_destroy(BarberShop* shop);
void shop_plan_initial_arrival(BarberShop* shop);
void shop_handle_event(BarberShop* shop, Event event);
void shop_check_invariants(const BarberShop* shop);
void shop_print_statistics( const BarberShop* shop);

#endif
