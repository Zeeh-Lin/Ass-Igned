#ifndef __AI_CLIENT_H_
#define __AI_CLIENT_H__

/**
 * Create a file called "api_key.h" in directory "include",
 * and define your own API key as the macro "MY_API_KEY".
 * Or you can remove this line: #include "api_key.h" ,
 * and replace MY_API_KEY with your own api key simply.
 */
#include "api_key.h"
#define AIC_API_KEY MY_API_KEY
#define AIC_URL     "https://api.deepseek.com/chat/completions"
#define AIC_MODEL   "deepseek-chat"

extern char* answer;

typedef struct MemoryStruct {
  char *memory;
  size_t size;
} MemoryStruct_t;

/**
 * @brief  Initializes the AI client and required libraries.
 * @return 0 on success; exits the program on failure.
 */
int aic_init(void);

/**
 * @brief Sends a request to the DeepSeek API.
 * * @param prompt The message to send to the AI.
 * @return A dynimically allocated string containing the AI's response.
 *         Returns NULL on failure.
 *         The caller is resbonsible for freeing the returned string.
 */
char* aic_call(const char *prompt);

char* aic_task_add_prompt(const char *task_input);
char* aic_task_update_prompt(const char *current_task_json, const char *instruction, time_t created_at);
char* aic_task_suggest_prompt(const char *task_list_json);
char* aic_report_prompt(const char *task_list_json, const char *report_type);

/**
 * @brief Releases all resources used by the AI client.
 */
void aic_cleanup(void);

#endif