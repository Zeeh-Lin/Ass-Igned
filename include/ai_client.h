#ifndef __AI_CLIENT_H_
#define __AI_CLIENT_H__

#define AIC_API_KEY "***REMOVED***"
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

/**
 * @brief Builds the AI prompt string for task addition by safely inserting the task_input into the template.
 * * @param task_input The user's task description string (char*).
 * @return A dynamically allocated string containing the full prompt. The caller is responsible for freeing the returned string using free(). Returns NULL on failure.
 */
char* aic_task_add_prompt(const char *task_input);

/**
 * @brief Releases all resources used by the AI client.
 */
void aic_cleanup(void);

#endif