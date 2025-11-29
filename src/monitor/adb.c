#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "adb.h"
#include "ai_client.h"

static int cmd_help(char *args);
static int cmd_q(char *args);
static int cmd_ai(char *args);
static int subcmd_ai_chat(char *args);
// static int cmd_rpt(char *args);
// static int cmd_add(char *args);
// static int cmd_del(char *args);
// static int cmd_done(char *args);
// static int cmd_update(char *args);
// static int cmd_ls(char *args);
// static int cmd_info(char *args);
// static int cmd_next(char *args);

static char* rl_gets() {
  static char *line_read = NULL;

  if (line_read) {
    free(line_read);
    line_read = NULL;
  }

  line_read = readline("(ass) ");

  if (line_read && *line_read) {
    add_history(line_read);
    log_write("(ass) %s\n", line_read);
  }

  return line_read;
}

static cmd_t cmd_table [] = {
  { "help", "Display information about all supported commands", cmd_help },
  { "q", "Quit Ass-Igned", cmd_q },
  { "ai", "Basic AI Commands", cmd_ai }
};

static cmd_t subcmd_ai_table [] = {
  { "chat", "Chat with AI", subcmd_ai_chat }
};

#define NR_CMD ARRLEN(cmd_table)
#define NR_SUBCMD_AI ARRLEN(subcmd_ai_table)

static int cmd_help(char *args) {
  /* extract the first argument */
  char *arg = strtok(NULL, " ");
  int i;

  if (arg == NULL) {
    /* no argument given */
    for (i = 0; i < NR_CMD; i ++) {
      _Log("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
    }
  }
  else {
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(arg, cmd_table[i].name) == 0) {
        _Log("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
        return 0;
      }
    }
    _Log("Unknown command '%s'\n", arg);
  }
  return 0;
}

static int cmd_q(char *args) {
  ass_state.state = ASS_QUIT;
  return -1;
}

static int cmd_ai(char *args) {
  char *subcmd = strtok(NULL, " ");
  int i;

  if (subcmd == NULL) {
    for (i = 0; i < NR_SUBCMD_AI; i ++) {
      _Log("%s - %s\n", subcmd_ai_table[i].name, subcmd_ai_table[i].description);
    }
  }
  else {
    char *subcmd_args = subcmd + strlen(subcmd) + 1;
    for (i = 0; i < NR_SUBCMD_AI; i ++) {
      if (strcmp(subcmd, subcmd_ai_table[i].name) == 0) {
        if (subcmd_ai_table[i].handler(subcmd_args) != 0) {
          Log("info %s ERROR.", args);
          return 0;
        }
        break;
      }
    }

    if (i == NR_SUBCMD_AI) { _Log("Unknown subcommand '%s'\n", subcmd); }
  }

  return 0;
}

static int subcmd_ai_chat(char *args) {
  SAFE_FREE(answer);

  answer = aic_call(args);
  if (answer == NULL) {
    Log("AI chat error");
    return -1;
  }
  _Log("%s\n", answer);

  SAFE_FREE(answer);
  return 0;
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

    if (i == NR_CMD) { _Log("Unknown command '%s'\n", cmd); }
  }
}

void adb_init() {
  /* Compile the regular expressions. */
  init_regex();
}