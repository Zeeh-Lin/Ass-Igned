#include <curl/curl.h>
#include "common.h"
#include "ai_client.h"
#include "cJSON.h" 

char *answer = NULL;

// --- Internal Functions ---

static bool is_initialized = false;

// Receives and records the data from libcurl
static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
  size_t realsize = size * nmemb;
  MemoryStruct_t *mem = (MemoryStruct_t *)userp;

  char *ptr = realloc(mem->memory, mem->size + realsize + 1);
  if (ptr == NULL) {
    // Handle memory allocation error
    return 0;
  }

  mem->memory = ptr;
  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;

  return realsize;
}

// Creates request JSON
static char* create_request_json(const char* prompt) {
  cJSON *root = cJSON_CreateObject();
  cJSON *messages = cJSON_CreateArray();
  char *json_string = NULL;

  if (!root || !messages) goto end;

  cJSON_AddStringToObject(root, "model", AIC_MODEL);
  
  // System role
  cJSON *sys_msg = cJSON_CreateObject();
  cJSON_AddStringToObject(sys_msg, "role", "system");
  cJSON_AddStringToObject(sys_msg, "content", "You are a helpful assistant.");
  cJSON_AddItemToArray(messages, sys_msg);

  // User role
  cJSON *user_msg = cJSON_CreateObject();
  cJSON_AddStringToObject(user_msg, "role", "user");
  cJSON_AddStringToObject(user_msg, "content", prompt);
  cJSON_AddItemToArray(messages, user_msg);

  cJSON_AddItemToObject(root, "messages", messages);
  cJSON_AddFalseToObject(root, "stream");

  json_string = cJSON_PrintUnformatted(root);

end:
  cJSON_Delete(root);
  return json_string; // Must be freed by the caller
}

// --- Public Functions ---

int aic_init(void) {
  if (is_initialized) return 0;
  
  // Initialize libcurl globally
  CURLcode res = curl_global_init(CURL_GLOBAL_DEFAULT);
  Assert(res == CURLE_OK,
    "curl_global_init() failed: %s", curl_easy_strerror(res));
  is_initialized = 1;
  return 0;
}

void aic_cleanup(void) {
  if (is_initialized) {
    curl_global_cleanup();
    is_initialized = 0;
  }
}

char* aic_call(const char *prompt) {
  Assert(is_initialized, "Error: aic_init() must be called first.");
  CURL *curl = NULL;
  CURLcode res;
  struct curl_slist *headers = NULL;
  MemoryStruct_t chunk = {NULL, 0};
  char auth_header[256];
  char *json_data = NULL;
  char *ai_response = NULL;

  // 1. Prepare
  chunk.memory = malloc(1); 
  if (!chunk.memory) {
    panic("Memory allocation failed for chunk.memory.");
    goto cleanup;
  }

  json_data = create_request_json(prompt);
  if (!json_data) {
    Log(ANSI_FMT("Failed to create JSON request body.", ANSI_FG_RED));
    goto cleanup;
  }

  // 2. Setup Headers
  snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", AIC_API_KEY);
  headers = curl_slist_append(headers, "Content-Type: application/json");
  headers = curl_slist_append(headers, auth_header);
  
  // 3. Setup and Execute libcurl
  curl = curl_easy_init();
  if (!curl) {
    Log(ANSI_FMT("curl_easy_init() failed.", ANSI_FG_RED));
    goto cleanup;
  }

  curl_easy_setopt(curl, CURLOPT_URL, AIC_URL);
  curl_easy_setopt(curl, CURLOPT_POST, 1L);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
  
  res = curl_easy_perform(curl);
  
  if(res != CURLE_OK) {
    Log(ANSI_FMT("curl_easy_perform() failed: %s", ANSI_FG_RED), curl_easy_strerror(res));
    goto cleanup;
  }

  // 4. Process Response
  cJSON *response_json = cJSON_Parse(chunk.memory);
  cJSON *choices, *first_choice, *message, *content;
  
  if (!response_json) {
    Log(ANSI_FMT("Failed to parse JSON response. Raw: %s",
      ANSI_FG_RED), chunk.memory);
    goto json_cleanup;
  }
  
  choices = cJSON_GetObjectItemCaseSensitive(response_json, "choices");
  if (!choices) {
    Log(ANSI_FMT("JSON structure error: Missing 'choices'. Raw: %s",
      ANSI_FG_RED), chunk.memory);
    goto json_cleanup;
  }

  first_choice = cJSON_GetArrayItem(choices, 0);
  if (!first_choice) {
    Log(ANSI_FMT("JSON structure error: Missing first choice item. Raw: %s",
      ANSI_FG_RED), chunk.memory);
    goto json_cleanup;
  }

  message = cJSON_GetObjectItemCaseSensitive(first_choice, "message");
  if (!message) {
    Log(ANSI_FMT("JSON structure error: Missing 'message'. Raw: %s", ANSI_FG_RED), chunk.memory);
    goto json_cleanup;
  }

  content = cJSON_GetObjectItemCaseSensitive(message, "content");
  
  if (cJSON_IsString(content) && content->valuestring != NULL) {
    // Duplicate the string so that cJSON_Delete doesn't free it
    ai_response = strdup(content->valuestring);
    if (!ai_response) { panic("Memory allocation failed for ai_response."); }
  } else {
    // Likely API error message in the response
    Log(ANSI_FMT("API error or content extraction failed. Full response:\n%s",
      ANSI_FG_RED), chunk.memory);
  }

json_cleanup:
  cJSON_Delete(response_json);

cleanup:
  // 5. Cleanup Resources
  if (curl) curl_easy_cleanup(curl);
  if (headers) curl_slist_free_all(headers);
  if (json_data) free(json_data);
  if (chunk.memory) free(chunk.memory);
  
  return ai_response; // Returns the duplicated answer string or NULL
}