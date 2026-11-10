#ifndef BARBERSHOP_H
#define BARBERSHOP_H
#include "types.h"

bool shop_init(const Config* config);
void shop_destroy(void);
bool shop_start(void);
void shop_stop(void);
void barber_process(Barber* barber);
void customer_process(Customer* customer);
double get_time(void);
void shop_print_stats(void);
#endif