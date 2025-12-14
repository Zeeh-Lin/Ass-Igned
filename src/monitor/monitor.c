#include <getopt.h>
#include "common.h"
#include "monitor.h"
#include "ai_client.h"
#include "database.h"

void adb_init();

ass_state_t ass_state = { .state = ASS_STOP };

static char *log_file = NULL;
static char *db_file = NULL;
static void welcome() {
  Log("Build time: %s, %s", __TIME__, __DATE__);
  _Log("Welcome to Ass-Igned!\n");
  _Log("     _                 ___                     _ \n");
  _Log("    / \\   ___ ___     |_ _|__ _ _ __   ___  __| |\n");
  _Log("   / _ \\ / __/ __|_____| |/ _` | '_ \\ / _ \\/ _` |\n");
  _Log("  / ___ \\\\__ \\__ \\_____| | (_| | | | |  __/ (_| |\n");
  _Log(" /_/   \\_\\___/___/    |___\\__, |_| |_|\\___|\\__,_|\n");
  _Log("                          |___/                  \n");
  _Log("For help, type \"help\"\n");
}

static int parse_args(int argc, char *argv[]) {
  const struct option table[] = {
    {"log"      , required_argument, NULL, 'l'},
    {"database" , required_argument, NULL, 'd'},
    {"help"     , no_argument      , NULL, 'h'},
    {0          , 0                , NULL,  0 },
  };
  int o;
  while ( (o = getopt_long(argc, argv, "-bhl:d:p:", table, NULL)) != -1) {
    switch (o) {
      case 'l': log_file = optarg; break;
      case 'd': db_file = optarg; break;
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
  db_init(db_file);
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
  if (db_save_db() != 0)
    Log("Database save error.");
  db_shutdown();
  aic_cleanup();
  log_close();
}