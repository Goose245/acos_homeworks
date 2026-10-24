#define _POSIX_C_SOURCE 200809L
#include "logger.h"
#include <stdio.h>
#include<stdarg.h>
#include <fcntl.h>
#include<unistd.h>
#include <pthread.h>

static int g_log_fd = -1;
static pthread_mutex_t g_log_mutex = PTHREAD_MUTEX_INITIALIZER;

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
  pthread_mutex_lock(&g_log_mutex);
  if (g_log_fd >= 0) {
    close(g_log_fd);
    g_log_fd = -1;
  }
  pthread_mutex_unlock(&g_log_mutex);
  pthread_mutex_destroy(&g_log_mutex);
}

void logger_log(const char* format, ...) {
  char buffer[1024];
  va_list args;
  va_start(args, format);
  int len = vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  if (len < 0) {
    return;
  }
  pthread_mutex_lock(&g_log_mutex);
  dprintf(STDOUT_FILENO, "%s\n", buffer);
  if (g_log_fd >= 0) {
    dprintf(g_log_fd, "%s\n",buffer);
  }
  pthread_mutex_unlock(&g_log_mutex);
}