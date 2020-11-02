#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "log.h"
#include "subprogram.h"

static int exec_count = 0;

static void child_reaper(int signal) {
  int wstatus = -1;
  pid_t pid = waitpid(-1, &wstatus, WNOHANG);

  for (; pid > 0; pid = waitpid(-1, &wstatus, WNOHANG)) {
    LOGF("PID [%d] exited code [%d]", pid, WEXITSTATUS(wstatus));
  }
}

void subprogram_start(const char *command) {
  char log_filename[255] = {0};
  int log_fd = -1;
  int pid = -1;

  snprintf(log_filename, 255, "%s/subprogram.%d.log", LOG_DIRNAME, exec_count++);
  log_fd = open(log_filename,
                O_TRUNC | O_CREAT | O_SYNC | O_WRONLY,
                0644);
  if (log_fd < 0) {
    LOGF("start_program: Failed to start [%s], couldn't open [%s] for writing: %s",
         command, log_filename, strerror(errno));
    return;
  }

  pid = fork();
  if (pid < 0) {
    LOGF("start_program: Failed to fork: %s", strerror(errno));
    return;
  } else if (pid > 0) {
    LOGF("start_program: Forked to PID [%d]", pid);
    close(log_fd);
    return;
  }

  close(fileno(stdin));
  dup2(log_fd, fileno(stderr));
  dup2(log_fd, fileno(stdout));

  LOGF("start_program: Split to [%s] as PID [%d] logfile [%s]",
       command, getpid(), log_filename);

  int result = execl("/bin/sh", "/bin/sh", "-c", command, NULL);
  if (result < 0) {
    LOGF("start_program: Failed to start [%s], couldn't exec: %s",
         command, strerror(errno));
    exit(1);
  }
}

void subprogram_init(void) {
  signal(SIGCHLD, child_reaper);
}
