#ifndef BARBERSHOP_LOGGER_H
#define BARBERSHOP_LOGGER_H
#include <stdbool.h>

bool logger_init(const char* log_file);
void logger_close(void);
void logger_log(double sim_time, const char* format, ...);


#endif
