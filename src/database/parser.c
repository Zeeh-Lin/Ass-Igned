#include "parser.h"
#include "database.h"
#include "common.h"
#include "cJSON.h"
#include <string.h>
#include <time.h>

// --- PRIVATE UTILITY ---

/**
 * @brief 内部函数：安全地从 JSON 字符串项复制到目标结构体字符串字段。
 */
static void _psr_copy_json_string(cJSON *item, char *dest, size_t max_len) {
    if (cJSON_IsString(item) && (item->valuestring != NULL)) {
        strncpy(dest, item->valuestring, max_len - 1);
        dest[max_len - 1] = '\0'; // 确保 null 终止
    }
}

// --- PARSER API IMPLEMENTATIONS ---

/**
 * @brief 从 JSON 字符串解析任务数据，填充到 task_t 结构体中。
 */
int psr_json_to_task(const char *task_json, task_t *task_out, int require_id) {
    cJSON *root = NULL;
    cJSON *item = NULL;
    int result = -1;
    
    // 确保目标结构体清零
    memset(task_out, 0, sizeof(task_t));

    root = cJSON_Parse(task_json);
    if (root == NULL) {
        Log("JSON ERROR: Failed to parse input JSON.");
        goto end;
    }

    // 1. 解析 ID (仅在 require_id 为真时需要)
    if (require_id) {
        item = cJSON_GetObjectItemCaseSensitive(root, "id");
        if (cJSON_IsNumber(item) && item->valueint > 0) {
            task_out->id = item->valueint;
        } else {
            Log("JSON ERROR: ID is required but missing or invalid.");
            goto end;
        }
    }
    
    // 2. 解析 Title (必填)
    item = cJSON_GetObjectItemCaseSensitive(root, "title");
    if (cJSON_IsString(item) && (item->valuestring != NULL)) {
        _psr_copy_json_string(item, task_out->title, TASK_TITLE_MAX_LEN);
    } else if (require_id == 0) {
        // 如果是新建任务，title 必填
        Log("JSON ERROR: 'title' is required for new tasks.");
        goto end;
    }

    // 3. 解析 Description (可选)
    item = cJSON_GetObjectItemCaseSensitive(root, "description");
    _psr_copy_json_string(item, task_out->description, TASK_DESC_MAX_LEN);
    
    // 4. 解析 Priority (可选)
    item = cJSON_GetObjectItemCaseSensitive(root, "prio");
    if (cJSON_IsNumber(item)) {
        int prio_val = item->valueint;
        if (prio_val >= PRIORITY_URGENT && prio_val <= PRIORITY_LOW) {
            task_out->prio = (task_priority_e)prio_val;
        } else {
            task_out->prio = PRIORITY_MEDIUM;
        }
    } else if (require_id == 0) {
        task_out->prio = PRIORITY_MEDIUM;
    }

    // 5. 解析 Status (可选，用于更新或加载)
    item = cJSON_GetObjectItemCaseSensitive(root, "status");
    if (cJSON_IsNumber(item)) {
        int stat_val = item->valueint;
        if (stat_val >= TASK_STATUS_TODO && stat_val <= TASK_STATUS_DELETED) {
            task_out->stat = (task_status_e)stat_val;
        } else {
            task_out->stat = TASK_STATUS_TODO;
        }
    } else if (require_id == 0) {
        task_out->stat = TASK_STATUS_TODO;
    }

    // 6. 解析时间戳 (可选，用于更新或加载)
    item = cJSON_GetObjectItemCaseSensitive(root, "due_date");
    if (cJSON_IsNumber(item)) {
        task_out->due_date = (time_t)item->valueint;
    }
    
    item = cJSON_GetObjectItemCaseSensitive(root, "completed_at");
    if (cJSON_IsNumber(item)) {
        task_out->completed_at = (time_t)item->valueint;
    }

    // 默认设置/更新 created_at (仅在新建任务时，否则应保持原值)
    if (require_id == 0) {
        task_out->created_at = time(NULL);
    }
    
    result = 0; // Success

end:
    if (root != NULL) {
        cJSON_Delete(root);
    }
    return result;
}

/**
 * @brief 将 task_t 结构体序列化为 JSON 字符串。
 */
char* psr_task_to_json(const task_t *task_in) {
    if (task_in == NULL) return NULL;
    
    cJSON *root = cJSON_CreateObject();
    char *json_string = NULL;

    if (root == NULL) {
        Log("JSON ERROR: Failed to create JSON root object.");
        return NULL;
    }

    // 1. 核心字段
    cJSON_AddNumberToObject(root, "id", task_in->id);
    cJSON_AddStringToObject(root, "title", task_in->title);
    cJSON_AddStringToObject(root, "description", task_in->description);
    
    // 2. 枚举/数字字段
    cJSON_AddNumberToObject(root, "prio", task_in->prio);
    cJSON_AddNumberToObject(root, "status", task_in->stat);
    
    // 3. 时间戳字段
    cJSON_AddNumberToObject(root, "created_at", (int)task_in->created_at);
    cJSON_AddNumberToObject(root, "due_date", (int)task_in->due_date);
    cJSON_AddNumberToObject(root, "completed_at", (int)task_in->completed_at);

    // 4. 序列化为字符串
    // 使用 cJSON_PrintUnformatted 节省空间，或 cJSON_Print 用于美化输出
    json_string = cJSON_Print(root);
    
    cJSON_Delete(root); // 清理 cJSON 树结构

    if (json_string == NULL) {
        Log("JSON ERROR: Failed to print JSON string.");
    }

    return json_string; // 返回的字符串需要调用者 free
}

/**
 * @brief 将 time_t 时间戳转换为人类可读的字符串格式。
 * @param timestamp 要转换的时间戳。
 * @param buffer 接收格式化后字符串的缓冲区。
 * @param buffer_size 缓冲区大小。
 */
void psr_readable_time(time_t timestamp, char *buffer, size_t buffer_size) {
    if (timestamp == 0) {
        // 如果时间戳为 0，表示时间未设置（例如 completed_at），打印 N/A
        strncpy(buffer, "N/A", buffer_size);
        buffer[buffer_size - 1] = '\0';
        return;
    }

    // 将 time_t 转换为本地时间结构
    struct tm *local_time = localtime(&timestamp);

    // 使用 strftime 格式化时间（例如: 2025-12-14 18:34:15）
    // 确保缓冲区足够大
    if (local_time != NULL) {
        strftime(buffer, buffer_size, "%Y-%m-%d %H:%M:%S", local_time);
    } else {
        strncpy(buffer, "Invalid Time", buffer_size);
        buffer[buffer_size - 1] = '\0';
    }
}

/**
 * @brief 将任务状态枚举转换为字符串。
 */
const char* psr_status_string(task_status_e status) {
    switch (status) {
        case TASK_STATUS_TODO: return "TODO";
        case TASK_STATUS_DOING: return "DOING";
        case TASK_STATUS_DONE: return "DONE";
        case TASK_STATUS_DELETED: return "DELETED (Archived)";
        default: return "UNKNOWN";
    }
}

/**
 * @brief 将任务优先级枚举转换为字符串。
 */
const char* psr_priority_string(task_priority_e priority) {
    switch (priority) {
        case PRIORITY_URGENT: return "0 - URGENT";
        case PRIORITY_IMPORTANT: return "1 - IMPORTANT";
        case PRIORITY_MEDIUM: return "2 - MEDIUM";
        case PRIORITY_LOW: return "3 - LOW";
        default: return "UNKNOWN";
    }
}