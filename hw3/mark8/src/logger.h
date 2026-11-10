#ifndef LOGGER_H
#define LOGGER_H
#include <stdbool.h>

bool logger_init(const char* log_file , const char* sem_name);

void logger_close(void);

void logger_log (const char* format, ...);

#endif
