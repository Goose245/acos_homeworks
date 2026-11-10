#define _POSIX_C_SOURCE 200809L
#include "logger.h"
#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>
#include <string.h>

static int g_log_fd = -1;
static sem_t* g_log_sem = SEM_FAILED;
static char g_sem_name[128] = {0};

bool logger_init(const char* log_file, const char* sem_name) {
  if (log_file != NULL) {
    g_log_fd = open(log_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (g_log_fd < 0) {
      perror("logger: fail to open file");
      return false;
    }
  }
  if (sem_name != NULL) {
    strncpy(g_sem_name, sem_name, sizeof(g_sem_name) - 1);
    g_log_sem = sem_open(g_sem_name,O_CREAT, 0644, 1);
    if (g_log_sem == SEM_FAILED) {
      perror("logger: fail to open semaphore");
      if (g_log_fd >= 0) {
        close(g_log_fd);
      }
      return false;
    }
  }
  return true;
}
void logger_close(void) {
  if (g_log_sem != SEM_FAILED) {
    sem_wait(g_log_sem);
    if (g_log_fd >= 0) {
      close(g_log_fd);
      g_log_fd = -1;
    }
    sem_post(g_log_sem);
    sem_close(g_log_sem);
    if (g_sem_name[0] != '\0') {
      sem_unlink(g_sem_name);
    }
    g_log_sem = SEM_FAILED;
  }
  else {
    if (g_log_fd >= 0) {
      close(g_log_fd);
      g_log_fd = -1;
    }
  }
}

void logger_log(const char* format , ...) {
  char buffer[1024];
  va_list args;
  va_start(args, format);
  int len = vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  if (len < 0 ) {
    return;
  }
  if (g_log_sem != SEM_FAILED) {
    sem_wait(g_log_sem);
  }
  dprintf(STDOUT_FILENO, "%s\n", buffer);
  if (g_log_fd >=  0 ) {
    dprintf(g_log_fd, "%s\n", buffer);
  }
  if (g_log_sem != SEM_FAILED) {
    sem_post(g_log_sem);
  }
}