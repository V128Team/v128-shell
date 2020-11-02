#ifndef LOG_H
#define LOG_H

#define LOG(fmt) {                              \
    fprintf(LOGFILE, fmt);                      \
    fprintf(LOGFILE, "\n");                     \
    fflush(LOGFILE);                            \
  }

#define LOGF(fmt, ...) {                        \
    fprintf(LOGFILE, fmt, __VA_ARGS__);         \
    fprintf(LOGFILE, "\n");                     \
    fflush(LOGFILE);                            \
  }

#define LOGFATAL(fmt)       do { LOG(fmt); exit(1); } while (0);
#define LOGFATALF(fmt, ...) do { LOGF(fmt, __VA_ARGS__); exit(1); } while (0);

#define LOG_DIRNAME  "/var/log/v128"
#define LOG_FILENAME "/var/log/v128/shell.log"

extern FILE* LOGFILE;
extern int   LOGFD;

extern void log_init(void);

#endif // LOG_H
