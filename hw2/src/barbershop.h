#ifndef BARBERSHOP_H
#define BARBERSHOP_H
#include "types.h"

bool shop_init(Barbershop* shop, const Config* config);
bool shop_start(Barbershop* shop);
void shop_stop(Barbershop* shop);
void shop_destroy(Barbershop* shop);
void *barbershop_thread(void *arg);
void *customer_thread(void *arg);
double get_time(const Barbershop* shop);
#endif
