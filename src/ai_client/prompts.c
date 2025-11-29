#include "common.h"
#include "ai_client.h"

// The template uses %s as a placeholder for the user's task input.
static const char *TASK_ADD_PROMPT_TEMPLATE =
    "You are an expert task parsing and structuring assistant. Your job is to accurately determine and extract four key attributes from a single user-provided task description: title, description, due_date, and priority.\n\n"
    "You **MUST** strictly adhere to the following data constraints and output format:\n\n"
    "### Data Constraints\n"
    "1.  **title**: The task's brief title. Max length 128 characters.\n"
    "2.  **description**: Detailed task information. Max length 256 characters.\n"
    "3.  **due_date**: The required completion time (deadline). **MUST** be a standard Unix timestamp (seconds since 1970-01-01 UTC).\n"
    "4.  **priority**: The task's priority. **MUST** use one of the following integer enum values:\n"
    "    * `0`: PRIORITY_URGENT (Highest priority, must be done now)\n"
    "    * `1`: PRIORITY_IMPORTANT\n"
    "    * `2`: PRIORITY_MEDIUM\n"
    "    * `3`: PRIORITY_LOW (Lowest priority)\n\n"
    "### Task Parsing Rules\n"
    "* **due_date**: If no explicit date or time is mentioned in the task description, set `due_date` to the Unix timestamp for UTC midnight (00:00:00) **one week from 2025-11-30** as a reasonable default.\n"
    "* **priority**:\n"
    "    * If the task contains words like “紧急”, “立刻”, “必须” (Urgent, Immediately, Must), set to `0` (URGENT).\n"
    "    * If the task contains words like “重要”, “尽快” (Important, ASAP), set to `1` (IMPORTANT).\n"
    "    * If the task contains words like “正常”, “常规” (Normal, Routine), set to `2` (MEDIUM).\n"
    "    * If the task contains words like “有空时再做”, “低优先级” (When free, Low priority), set to `3` (LOW).\n"
    "    * If no clear instruction is provided, default to `2` (MEDIUM).\n"
    "* **Output Format**: The final output **MUST ONLY** be a JSON object, without any additional explanation, notes, code block markers (like ```json), or extra text.\n\n"
    "--- \n"
    "### Example\n\n"
    "**输入任务**: “立刻停止所有工作，将今天早上 9 点前收集到的所有数据备份到云端，这个事情优先级最高，不能拖延。”\n\n"
    "**Expected Output (JSON)**:\n"
    "{\"title\": \"备份今日收集数据到云端\", \"description\": \"停止所有工作，将今天早上 9 点前收集到的所有数据备份到云端。\", \"due_date\": 1764531600, \"priority\": 0}\n"
    "--- \n\n"
    "### Task to Parse\n"
    "%s"
    "\n\n### Expected Output (JSON)\n";

/**
 * @brief Builds the AI prompt string for task addition by safely inserting the task_input into the template.
 * * @param task_input The user's task description string (char*).
 * @return A dynamically allocated string containing the full prompt. The caller is responsible for freeing the returned string using free(). Returns NULL on failure.
 */
char* aic_task_add_prompt(const char *task_input) {
    if (!task_input) {
        return NULL;
    }

    // 1. Estimate required total length.
    size_t template_len = strlen(TASK_ADD_PROMPT_TEMPLATE);
    size_t task_len = strlen(task_input);
    
    // Add 128 bytes buffer for safety, plus 1 for null terminator.
    size_t required_len = template_len + task_len + 128 + 1;
    
    // 2. Allocate memory on the heap.
    char *full_prompt = (char*)malloc(required_len);
    if (full_prompt == NULL) {
        return NULL; // Memory allocation failure
    }

    // 3. Use snprintf to safely construct the string by inserting task_input into %s.
    int written = snprintf(full_prompt, required_len, TASK_ADD_PROMPT_TEMPLATE, task_input);

    // 4. Check for snprintf error or truncation.
    if (written < 0 || (size_t)written >= required_len) {
        free(full_prompt);
        return NULL;
    }

    // Success, return the dynamically allocated string.
    return full_prompt;
}