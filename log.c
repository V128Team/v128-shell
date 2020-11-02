#include <errno.h>
#include <execinfo.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "log.h"

FILE* LOGFILE = NULL;
int   LOGFD   = -1;

static void error_handler(int signal) {
  void *array[10];
  size_t size;
  size = backtrace(array, 10);

  LOGF("Error: signal %d:", signal);
  backtrace_symbols_fd(array, size, LOGFD);
  exit(1);
}

void log_init(void) {
  LOGFILE = stderr;
  LOGFD = open(LOG_FILENAME,
               O_CLOEXEC | O_TRUNC | O_CREAT | O_SYNC | O_WRONLY,
               0644);

  if (LOGFD < 0) {
    fprintf(stderr, "Couldn't open log file [%s]: %s\n", LOG_FILENAME, strerror(errno));
    exit(1);
  }

  LOGFILE = fdopen(LOGFD, "w");

  // Catch SIGSEGVs
  signal(SIGSEGV, error_handler);

  // Reroute stdout and stderr to log file
  dup2(LOGFD, fileno(stdout));
  dup2(LOGFD, fileno(stderr));
}
