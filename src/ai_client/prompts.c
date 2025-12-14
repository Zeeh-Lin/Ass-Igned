// ai_client.c

#include "common.h"
#include "ai_client.h"
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "parser.h"

// New template now accepts THREE format placeholders: 
// %s (Task Input), %ld (Current Unix Time), %s (Current Human-Readable Time)
static const char *TASK_ADD_PROMPT_TEMPLATE =
    "You are an expert task parsing and structuring assistant. Your job is to accurately determine and extract four key attributes from a single user-provided task description: title, description, due_date, and priority.\n\n"
    "### Current Time Context\n"
    "The current system time is: **%ld** (Unix Timestamp) / **%s** (UTC Readable Time).\n"
    "Use this context to accurately resolve relative deadlines (e.g., 'next Monday', 'in 3 days').\n\n"
    "You **MUST** strictly adhere to the following data constraints and output format:\n\n"
    "### Data Constraints\n"
    "1. **title**: The task's brief title. Max length 128 characters.\n"
    "2. **description**: Detailed task information. Max length 256 characters.\n"
    "3. **due_date**: The required completion time (deadline). **MUST** be a standard Unix timestamp (seconds since 1970-01-01 UTC).\n"
    "4. **priority**: The task's priority. **MUST** use one of the following integer enum values:\n"
    " * `0`: PRIORITY_URGENT\n"
    " * `1`: PRIORITY_IMPORTANT\n"
    " * `2`: PRIORITY_MEDIUM\n"
    " * `3`: PRIORITY_LOW\n\n"
    "### Task Parsing Rules\n"
    "* **due_date**: If no explicit date or time is mentioned in the task description, set `due_date` to the Unix timestamp for UTC midnight (00:00:00) **one week from the current time** as a reasonable default.\n"
    "* **priority**: Use the standard Chinese keywords mapping (紧急->0, 重要->1, 正常->2, 低优先级->3). Default to `2`.\n"
    "* **Output Format**: The final output **MUST ONLY** be a JSON object, without any additional explanation, notes, code block markers, or extra text.\n\n"
    "--- \n"
    "### Task to Parse\n"
    "%s" // Placeholder for task_input
    "\n\n### Expected Output (JSON)\n";

char* aic_task_add_prompt(const char *task_input) {
    if (!task_input) {
        return NULL;
    }
    
    time_t current_time = time(NULL); 
    
    char time_buffer[64];
    psr_readable_time(current_time, time_buffer, sizeof(time_buffer)); 

    size_t template_len = strlen(TASK_ADD_PROMPT_TEMPLATE);
    size_t task_len = strlen(task_input);
    size_t time_str_len = strlen(time_buffer);
    
    size_t required_len = template_len + task_len + time_str_len + 20 + 128 + 1; 
    
    char *full_prompt = (char*)malloc(required_len);
    if (full_prompt == NULL) {
        return NULL; // Memory allocation failure
    }

    int written = snprintf(full_prompt, required_len, 
                           TASK_ADD_PROMPT_TEMPLATE, 
                           (long)current_time,      // %ld (Unix Timestamp)
                           time_buffer,             // %s (Readable Time)
                           task_input);             // %s (Task Input)

    if (written < 0 || (size_t)written >= required_len) {
        free(full_prompt);
        return NULL;
    }

    // Success
    return full_prompt;
}

static const char *TASK_UPDATE_PROMPT_TEMPLATE =
    "You are an expert task modification assistant. Your primary goal is to take the user's update instruction and the current task's state, apply the necessary changes, and return the **COMPLETE, MODIFIED TASK OBJECT**.\n\n"
    "### Current Time Context\n"
    "The current system time is: **%ld** (Unix Timestamp) / **%s** (UTC Readable Time).\n"
    "Use this context to accurately resolve relative deadlines (e.g., '明天下午').\n\n"
    "### Data Constraints\n"
    "1.  **Output Requirement**: You MUST return the **FULL JSON OBJECT** for the task after modification.\n"
    "2.  **ID Integrity**: The `id` field in the original JSON **MUST NOT BE CHANGED** under any circumstance. Preserve the original `id` value.\n"
    "3.  **Unmodified Fields**: Any field not mentioned in the update instruction (e.g., `title`, `description`, `prio`, `due_date`) MUST retain its original value from the Current Task State.\n"
    "4.  **Status/Priority Encoding**: Status (`stat`) and Priority (`prio`) must use their corresponding integer enum values.\n\n"
    "### Current Task State (JSON)\n"
    "%s\n\n" // Placeholder for current_task_json
    "### Update Instruction\n"
    "User's instruction for modification: %s\n\n" // Placeholder for instruction
    "### Expected Output (COMPLETE Modified JSON)\n";

char* aic_task_update_prompt(const char *current_task_json, const char *instruction) {
    if (!current_task_json || !instruction) {
        return NULL;
    }

    // 1. Get the current time (time_t) internally
    time_t current_time = time(NULL); 
    
    // 2. Format the current time into a readable string
    char time_buffer[64];
    // Assumes psr_readable_time is accessible
    psr_readable_time(current_time, time_buffer, sizeof(time_buffer)); 

    // 3. Calculate required length
    size_t template_len = strlen(TASK_UPDATE_PROMPT_TEMPLATE);
    size_t json_len = strlen(current_task_json);
    size_t instruction_len = strlen(instruction);
    size_t time_str_len = strlen(time_buffer);

    // Estimation: template + json + instruction + readable time + long timestamp string + buffer
    size_t required_len = template_len + json_len + instruction_len + time_str_len + 20 + 128 + 1; 
    
    // 4. Allocate memory
    char *full_prompt = (char*)malloc(required_len);
    if (full_prompt == NULL) {
        return NULL; // Memory allocation failure
    }

    // 5. Construct the string using snprintf
    int written = snprintf(full_prompt, required_len, 
                           TASK_UPDATE_PROMPT_TEMPLATE, 
                           (long)current_time,      // %ld (Unix Timestamp)
                           time_buffer,             // %s (Readable Time)
                           current_task_json,       // %s (Current Task JSON)
                           instruction);            // %s (Instruction)

    // 6. Check for snprintf errors
    if (written < 0 || (size_t)written >= required_len) {
        free(full_prompt);
        return NULL;
    }

    // Success
    return full_prompt;
}

static const char *TASK_SUGGEST_PROMPT_TEMPLATE =
    "You are an expert task scheduling and prioritization assistant. Your goal is to analyze the provided list of tasks and recommend the single most important and urgent task that should be completed next. Focus on: **URGENCY** (due dates) and **PRIORITY** levels.\n\n"
    "### Current Task List (JSON Array)\n"
    "%s\n\n" // Placeholder for task_list_json
    "### Recommendation Requirement\n"
    "1. **Analysis**: Briefly justify why this task is the best choice (e.g., 'Due date is today' or 'Highest priority and blocking other tasks').\n"
    "2. **Output**: State the recommended task's ID, Title, and Description.\n"
    "3. **Format**: The output MUST be in a human-readable, formatted text block, NOT a JSON object.\n\n"
    "### Expected Output\n";

char* aic_task_suggest_prompt(const char *task_list_json) {
    if (!task_list_json) return NULL;
    
    // Allocate memory and construct the prompt similar to previous implementations
    size_t template_len = strlen(TASK_SUGGEST_PROMPT_TEMPLATE);
    size_t json_len = strlen(task_list_json);
    size_t required_len = template_len + json_len + 128 + 1; // 128 byte buffer
    
    char *full_prompt = (char*)malloc(required_len);
    if (full_prompt == NULL) return NULL;

    int written = snprintf(full_prompt, required_len, 
                           TASK_SUGGEST_PROMPT_TEMPLATE, 
                           task_list_json);

    if (written < 0 || (size_t)written >= required_len) {
        free(full_prompt);
        return NULL;
    }

    return full_prompt;
}