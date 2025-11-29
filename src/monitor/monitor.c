#include <getopt.h>
#include "common.h"
#include "monitor.h"
#include "ai_client.h"

void adb_init();

ass_state_t ass_state = { .state = ASS_STOP };

static char *log_file = NULL;

static void welcome() {
  Log("Build time: %s, %s", __TIME__, __DATE__);
  _Log("Welcome to Ass-Igned!\n");
  _Log("For help, type \"help\"\n");
}

static int parse_args(int argc, char *argv[]) {
  const struct option table[] = {
    {"log"      , required_argument, NULL, 'l'},
    {"help"     , no_argument      , NULL, 'h'},
    {0          , 0                , NULL,  0 },
  };
  int o;
  while ( (o = getopt_long(argc, argv, "-bhl:d:p:", table, NULL)) != -1) {
    switch (o) {
      case 'l': log_file = optarg; break;
      default:
        printf("Usage: %s [OPTION...] IMAGE [args]\n\n", argv[0]);
        printf("\t-l,--log=FILE           output log to FILE\n");
        printf("\n");
        exit(0);
    }
  }
  return 0;
}

void monitor_init(int argc, char *argv[]) {
  parse_args(argc, argv);
  log_init(log_file);
  adb_init();
  Assert(aic_init() == 0, "AI Client init error.");
  welcome();
}

void monitor_cleanup() {
  switch (ass_state.state) {
    case ASS_RUNNING: ass_state.state = ASS_STOP; break;

    case ASS_END: case ASS_ABORT:
      Log("ass: %s",
          (ass_state.state == ASS_ABORT ? ANSI_FMT("ABORT", ANSI_FG_RED) :
           (ass_state.halt_ret == 0 ? ANSI_FMT("HIT GOOD TRAP", ANSI_FG_GREEN) :
            ANSI_FMT("HIT BAD TRAP", ANSI_FG_RED))));
      // fall through
    case ASS_QUIT: log_statistic();
  }

  aic_cleanup();
  log_close();
}