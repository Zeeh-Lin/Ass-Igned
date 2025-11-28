#include <log.h>

FILE *log_fp = NULL;

void log_init(const char *log_file) {
  log_fp = stdout;
  if (log_file != NULL) {
    FILE *fp = fopen(log_file, "w");
    Assert(fp, "Can not open '%s'", log_file);
    log_fp = fp;
  }
  Log("Log is written to %s", log_file ? log_file : "stdout");
}

// Collects runtime data for later use 
void log_statistic() {
  // extern g_timer
  // if (g_timer > 0)
  //   Log("host time spent = %ld us", g_timer);
  // else
  //   Log("Finish running in less than 1 us and can not calculate the simulation frequency");
}

bool log_enable() {
  // TODO： log when condition is true
  return true;
}
