#include "common.h"
#include "monitor.h"

int main(int argc, char *argv[]) {
  monitor_init(argc, argv);
  adb_mainloop();
  monitor_cleanup();

  return 0;
}
