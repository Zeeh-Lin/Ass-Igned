#include <curl/curl.h>
#include "ai_client.h" // 包含我们自己的头文件
#include "cJSON.h" // 假定 cJSON.h 在搜索路径中

// --- 内部结构体定义 (Internal Struct) ---

// 用于存储 API 响应数据的结构体
struct MemoryStruct {
    char *memory;
    size_t size;
};

// --- 内部函数实现 (Internal Implementation) ---

// libcurl 全局初始化只需一次
static int is_initialized = 0;

// 回调函数：接收并存储 libcurl 返回的数据
static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;

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

// 构造 JSON 请求体
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


// --- 3. 接口函数实现 (Public Function Implementations) ---

int aic_init(void) {
    if (is_initialized) return 0;
    
    // Initialize libcurl globally
    CURLcode res = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (res != CURLE_OK) {
        fprintf(stderr, "curl_global_init() failed: %s\n", curl_easy_strerror(res));
        return 1;
    }
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
    if (!is_initialized) {
        fprintf(stderr, "Error: aic_init() must be called first.\n");
        return NULL;
    }

    CURL *curl = NULL;
    CURLcode res;
    struct curl_slist *headers = NULL;
    struct MemoryStruct chunk = {NULL, 0};
    char auth_header[256];
    char *json_data = NULL;
    char *ai_response = NULL;

    // 1. Prepare
    chunk.memory = malloc(1); 
    if (!chunk.memory) { fprintf(stderr, "Memory allocation failed.\n"); goto cleanup; }

    json_data = create_request_json(prompt);
    if (!json_data) { fprintf(stderr, "Failed to create JSON request body.\n"); goto cleanup; }

    // 2. Setup Headers
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", AIC_API_KEY);
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, auth_header);
    
    // 3. Setup and Execute libcurl
    curl = curl_easy_init();
    if (!curl) { fprintf(stderr, "curl_easy_init() failed.\n"); goto cleanup; }

    curl_easy_setopt(curl, CURLOPT_URL, AIC_URL);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    
    res = curl_easy_perform(curl);
    
    if(res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        goto cleanup;
    }

    // 4. Process Response
    cJSON *response_json = cJSON_Parse(chunk.memory);
    cJSON *choices, *first_choice, *message, *content;
    
    if (!response_json) {
        fprintf(stderr, "Failed to parse JSON response. Raw: %s\n", chunk.memory);
        goto json_cleanup;
    }
    
    choices = cJSON_GetObjectItemCaseSensitive(response_json, "choices");
    first_choice = cJSON_GetArrayItem(choices, 0);
    message = cJSON_GetObjectItemCaseSensitive(first_choice, "message");
    content = cJSON_GetObjectItemCaseSensitive(message, "content");
    
    if (cJSON_IsString(content) && content->valuestring != NULL) {
        // Duplicate the string so that cJSON_Delete doesn't free it
        ai_response = strdup(content->valuestring); 
    } else {
        // Likely API error message in the response
        fprintf(stderr, "API error or content extraction failed. Full response:\n%s\n", chunk.memory);
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