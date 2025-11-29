#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "adb.h"

static int cmd_h(char *args);
static int cmd_q(char *args);

static char* rl_gets() {
  static char *line_read = NULL;

  if (line_read) {
    free(line_read);
    line_read = NULL;
  }

  line_read = readline("(ass) ");

  if (line_read && *line_read) {
    add_history(line_read);
  }

  return line_read;
}

static struct {
  const char *name;
  const char *description;
  int (*handler) (char *);
} cmd_table [] = {
  { "h", "Display information about all supported commands", cmd_h },
  { "q", "Exit Ass-Igned", cmd_q }
};

#define NR_CMD ARRLEN(cmd_table)
#define NR_SUBCMD_INFO ARRLEN(subcmd_info_table)

static int cmd_h(char *args) {
  /* extract the first argument */
  char *arg = strtok(NULL, " ");
  int i;

  if (arg == NULL) {
    /* no argument given */
    for (i = 0; i < NR_CMD; i ++) {
      printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
    }
  }
  else {
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(arg, cmd_table[i].name) == 0) {
        printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
        return 0;
      }
    }
    printf("Unknown command '%s'\n", arg);
  }
  return 0;
}

static int cmd_q(char *args) {
  ass_state.state = ASS_QUIT;
  return -1;
}

void adb_mainloop() {
  for (char *str; (str = rl_gets()) != NULL; ) {
    char *str_end = str + strlen(str);

    /* extract the first token as the command */
    char *cmd = strtok(str, " ");
    if (cmd == NULL) { continue; }

    /* treat the remaining string as the arguments,
     * which may need further parsing
     */
    char *args = cmd + strlen(cmd) + 1;
    if (args >= str_end) {
      args = NULL;
    }

    int i;
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(cmd, cmd_table[i].name) == 0) {
        if (cmd_table[i].handler(args) < 0) { return; }
        break;
      }
    }

    if (i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }
  }
}

void adb_init() {
  /* Compile the regular expressions. */
  init_regex();
}