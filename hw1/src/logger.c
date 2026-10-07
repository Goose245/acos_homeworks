#include "logger.h"
#include <stdio.h>
#include<stdarg.h>
#include <fcntl.h>
#include<unistd.h>

static int g_log_fd = -1;

bool logger_init(const char* log_file) {
  if (log_file != NULL) {
    g_log_fd = open(log_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (g_log_fd < 0) {
      perror("error opening log file");
      return false;
    }
  }
  return true;
}
void logger_close (void) {
  if (g_log_fd >= 0) {
    close(g_log_fd);
    g_log_fd = -1;
  }
}

void logger_log(double sim_time, const char* format, ...) {
  char buffer[1024];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  dprintf(STDOUT_FILENO, "[Время %6.2fs] %s\n", sim_time,buffer);
  if (g_log_fd >= 0) {
    dprintf(g_log_fd, "[Время %6.2fs] %s\n", sim_time,buffer);
  }
}