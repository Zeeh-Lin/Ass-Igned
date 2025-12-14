#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "adb.h"
#include "ai_client.h"
#include "database.h"
#include "parser.h"

static int cmd_dispatch(cmd_t *subcmd_table, int NR_SUBCMD, char *args);
static int cmd_help(char *args);
static int cmd_quit(char *args);

static int cmd_task(char *args);
static int subcmd_task_add(char *args);
static int subcmd_task_list(char *args);
static int subcmd_task_del(char *args);
static int subcmd_task_update(char *args);

static int cmd_ai(char *args);
static int subcmd_ai_chat(char *args);
static int subcmd_ai_sug(char *args);

static int cmd_report(char *args);
static int subcmd_report_w(char *args);
static int subcmd_report_m(char *args);
static cmd_t cmd_table [] = {
  { "help"  , "Display information about all supported commands", cmd_help },
  { "quit"  , "Quit Ass-Igned", cmd_quit },
  { "task"  , "Basic task commands", cmd_task },
  { "ai"    , "Basic AI commands", cmd_ai },
  { "report", "Generate weekly/monthly summary reports", cmd_report }
};

static cmd_t subcmd_task_table [] = {
  { "list"    , "List all tasks", subcmd_task_list },
  { "add"     , "Add a task", subcmd_task_add },
  { "del"     , "Delete a tasks", subcmd_task_del },
  { "update"  , "Delete a tasks", subcmd_task_update },
};

static cmd_t subcmd_ai_table [] = {
  { "chat", "Chat with AI", subcmd_ai_chat },
  { "sug", "Get AI suggestion for the next task", subcmd_ai_sug }
};

static cmd_t subcmd_report_table [] = {
  { "weekly", "Generate a weekly task summary report", subcmd_report_w },
  { "monthly", "Generate a monthly task summary report", subcmd_report_m }
};

#define NR_CMD         ARRLEN(cmd_table)
#define NR_SUBCMD(x)   ARRLEN(subcmd_ ## x ## _table)

static char* rl_gets() {
  static char *line_read = NULL;

  if (line_read) {
    free(line_read);
    line_read = NULL;
  }

  line_read = readline(ANSI_FMT("(ass) ", ANSI_FG_GREEN));

  if (line_read && *line_read) {
    add_history(line_read);
    log_write(ANSI_FMT("(ass) ", ANSI_FG_GREEN) "%s\n", line_read);
  }

  return line_read;
}

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

static int cmd_quit(char *args) {
  ass_state.state = ASS_QUIT;
  return -1;
}

static int cmd_dispatch(cmd_t *subcmd_table, int NR_SUBCMD, char *args) {
  char *subcmd = strtok(NULL, " ");
  int i;

  if (subcmd == NULL) {
    for (i = 0; i < NR_SUBCMD; i ++) {
      _Log("%s - %s\n", subcmd_table[i].name, subcmd_table[i].description);
    }
  }
  else {
    char *subcmd_args = subcmd + strlen(subcmd) + 1;
    for (i = 0; i < NR_SUBCMD; i ++) {
      if (strcmp(subcmd, subcmd_table[i].name) == 0) {
        if (subcmd_table[i].handler(subcmd_args) != 0) {
          Log("error subcommand");
          return 0;
        }
        break;
      }
    }

    if (i == NR_SUBCMD) { _Log("Unknown subcommand '%s'\n", subcmd); }
  }

  return 0;
}

static int cmd_task(char *args) {
  return cmd_dispatch(subcmd_task_table, NR_SUBCMD(task), args);
}

static int subcmd_task_add(char *args) {
  SAFE_FREE(answer);

  if (args == NULL || *args == '\0') {
    _Log("Usage: task add <prompt>\n");
    return -1;
  }

  char* prompt = NULL;
  prompt = aic_task_add_prompt(args);
  if (prompt == NULL) {
    Log("Failed to build prompt");
    return -1;
  }

  answer = aic_call(prompt);

  if (answer == NULL) {
    Log("AI task add error");
    return -1;
  }
  
  db_add_task(answer);

  SAFE_FREE(answer);
  free(prompt);
  return 0;
}

static int subcmd_task_list(char *args) {
  db_print_all_task();
  return 0;
}

static int subcmd_task_del(char *args) {
    // Check if arguments are provided
    if (args == NULL || *args == '\0') {
        _Log("Usage: task delete <task_id>\n");
        return -1;
    }

    // Attempt to convert the argument to an integer ID
    int id = atoi(args);
    if (id <= 0) {
        _Log("Error: Invalid task ID '%s'. ID must be a positive integer.\n", args);
        return -1;
    }

    // Call the database function to delete the task
    if (db_delete_task_by_id(id) == 0) {
        _Log("Success: Task ID %d deleted.\n", id);
        // Optionally list remaining tasks
        // subcmd_task_list(NULL); 
    } else {
        // db_delete_task_by_id already logs an error if ID is not found.
        _Log("Error: Failed to delete Task ID %d (check database logs).\n", id);
        return -1;
    }
    return 0;
}

static int subcmd_task_update(char *args) {
  // args should look like: "<task_id> <update_instruction>"
  
  char *id_str = strtok(args, " ");
  if (id_str == NULL) {
    _Log("Error: Missing task ID.\n");
    return -1;
  }

  int id = atoi(id_str);
  if (id <= 0) {
    _Log("Error: Invalid task ID '%s'.\n", id_str);
    return -1;
  }
    
  char *instruction = id_str + strlen(id_str) + 1;
  if (instruction == NULL || *instruction == '\0') {
    _Log("Error: Missing update instruction.\n");
    return -1;
  }

  task_t old_task;
  char *old_task_json = NULL;
  char *prompt = NULL;

  if (db_find_task_by_id(id, &old_task) != 0) {
    _Log("Error: Task ID %d not found. Update failed.\n", id);
    return -1;
  }
    
  old_task_json = psr_task_to_json(&old_task); 
  if (old_task_json == NULL) {
    Log("Error: Failed to serialize task ID %d to JSON.", id);
    return -1; // Cannot continue without JSON context
  }

  SAFE_FREE(answer);
  
  prompt = aic_task_update_prompt(old_task_json, instruction); 
  
  if (prompt == NULL) {
    Log("Failed to build update prompt for ID %d", id);
    SAFE_FREE(old_task_json);
    return -1;
  }

  answer = aic_call(prompt);

  if (answer == NULL) {
    Log("AI task update error (returned NULL).");
    SAFE_FREE(old_task_json);
    free(prompt);
    return -1;
  }
  
  // AI is instructed to return the COMPLETE, MODIFIED task JSON.
  // We use psr_json_to_task to overwrite 'old_task' with the new data.
  task_t new_task;
    
  // We pass require_id=1 to ensure the AI's output contains a valid ID 
  // (which should be the original ID).
  if (psr_json_to_task(answer, &new_task, 1) != 0) {
    Log("Error: AI returned invalid JSON or ID was missing/invalid.");
    Log("Bad JSON from AI: %s\n", answer); 
    SAFE_FREE(answer);
    SAFE_FREE(old_task_json);
    free(prompt);
    return -1;
  }

  // Crucial check: Ensure AI did not change the ID
  if (new_task.id != id) {
    _Log("FATAL ERROR: AI returned JSON with changed ID (%d -> %d). Aborting update.\n", id, new_task.id);
    SAFE_FREE(answer);
    SAFE_FREE(old_task_json);
    free(prompt);
    return -1;
  }

  if (db_update_task(&new_task) != 0) {
    Log("Update failed. Database write error for ID %d.\n", id);
    SAFE_FREE(answer);
    SAFE_FREE(old_task_json);
    free(prompt);
    return -1;
  }

  Log("Task ID %d updated successfully based on instruction: '%s'.", id, instruction);

  SAFE_FREE(answer);
  SAFE_FREE(old_task_json);
  free(prompt);
  return 0;
}

static int cmd_ai(char *args) {
  return cmd_dispatch(subcmd_ai_table, NR_SUBCMD(ai), args);
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

static int subcmd_ai_sug(char *args) {
  // args is currently unused for task suggestion, but included for completeness.
  
  char *task_list_json = NULL;
  char *prompt = NULL;

  // This calls the function implemented in the database layer.
  task_list_json = db_get_all_tasks_json(); 

  if (task_list_json == NULL || strcmp(task_list_json, "[]") == 0) {
    _Log("INFO: No active tasks found. Nothing to suggest.\n");
    // If task_list_json is "[]" (empty array), we should free it.
    SAFE_FREE(task_list_json); 
    return 0; // Success, but nothing to do
  }

  SAFE_FREE(answer);
  
  prompt = aic_task_suggest_prompt(task_list_json);
  
  if (prompt == NULL) {
    Log("Failed to build suggestion prompt.");
    SAFE_FREE(task_list_json);
    return -1;
  }

  _Log("INFO: Asking AI for next task suggestion...\n");
  answer = aic_call(prompt);

  if (answer == NULL) {
    Log("AI suggestion error (returned NULL).");
    SAFE_FREE(prompt);
    SAFE_FREE(task_list_json);
    return -1;
  }
  
  Log("\n=== AI Suggested Next Task ===\n%s\n", answer);

  // Clean up dynamically allocated memory
  SAFE_FREE(answer);
  SAFE_FREE(prompt);
  SAFE_FREE(task_list_json);
  
  return 0;
}

static int cmd_report(char *args) {
  return cmd_dispatch(subcmd_report_table, ARRLEN(subcmd_report_table), args);
}

static int generate_report(const char *report_type) {
    char *task_list_json = db_get_all_tasks_json(); 

    if (task_list_json == NULL || strcmp(task_list_json, "[]") == 0) {
        _Log("INFO: No tasks to generate %s report.\n", report_type);
        SAFE_FREE(task_list_json); 
        return 0;
    }
    
    SAFE_FREE(answer);
    char *prompt = aic_report_prompt(task_list_json, report_type); 
    
    if (prompt == NULL) {
        Log("Failed to build %s report prompt.", report_type);
        SAFE_FREE(task_list_json);
        return -1;
    }

    _Log("INFO: Generating %s report...\n", report_type);
    answer = aic_call(prompt);

    if (answer == NULL) {
        Log("AI report generation error for %s.", report_type);
        SAFE_FREE(prompt);
        SAFE_FREE(task_list_json);
        return -1;
    }
    
    _Log("\n=== %s Report ===\n%s\n", report_type, answer);

    SAFE_FREE(answer);
    SAFE_FREE(prompt);
    SAFE_FREE(task_list_json);
    return 0;
}

static int subcmd_report_w(char *args) {
  return generate_report("WEEKLY");
}

static int subcmd_report_m(char *args) {
  return generate_report("MONTHLY");
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