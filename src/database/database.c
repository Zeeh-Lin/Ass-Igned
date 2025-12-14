#include "database.h"
#include "index_manager.h"
#include "storage_manager.h"
#include "parser.h"
#include "common.h"

#define TIME_STR_LEN 30 // 定义时间字符串缓冲区大小
#define ESTIMATED_TASK_JSON_SIZE 1024
// --- DATABASE LIFECYCLE MANAGEMENT FUNCTIONS ---

/**
 * @brief 初始化数据库。
 * * 调用索引层的初始化函数，加载文件头和索引/空闲列表到内存。
 */
int db_init(const char* db_file) {
    if (idx_init(db_file) != 0) {
        Log("FATAL: Database initialization failed at index layer.");
        return -1;
    }
    Log("Database loaded successfully.");
    return 0;
}

/**
 * @brief 保存当前内存状态 (索引和Header) 到数据库文件。
 * * 任务数据块在 CRUD 操作时被立即写入，此函数仅用于同步元数据。
 */
int db_save_db(void) {
    // 假设 index_manager 提供了 save 接口，否则此函数只能作为占位符
    // 或依赖于 db_shutdown 来完成持久化。
    // 为了简化，我们依赖 db_shutdown 集中处理。
    Log("WARN: db_save_db only performs soft-save. Full persistence occurs at shutdown.");
    return 0; 
}

/**
 * @brief 清理所有内存分配并关闭文件。
 */
void db_shutdown(void) {
    Log("INFO: Shutting down database and persisting data...");
    // idx_shutdown 负责将内存数据写回文件 (Header/Index/Free List) 并关闭文件句柄。
    idx_shutdown();
    Log("INFO: Database successfully shut down.");
}


// --- TASK OPERATION (CRUD) FUNCTIONS ---

/**
 * @brief 添加一个新的任务记录。
 */
int db_add_task(const char *task_json) {
    task_t new_task;
    long allocated_offset;
    int new_id;

    // 1. 解析 JSON 并填充 task_t 结构体 (使用 Parser Layer)
    // 0 表示新建任务，不需要 ID
    if (psr_json_to_task(task_json, &new_task, 0) != 0) {
        Log("ERROR: Failed to parse task JSON for creation.");
        return -1;
    }

    // 2. 分配 ID
    new_id = idx_get_next_id();
    if (new_id <= 0) {
        Log("ERROR: Failed to get next available ID.");
        return -1;
    }
    new_task.id = new_id;

    // 3. 从 Free List 或文件末尾分配文件空间
    allocated_offset = idx_allocate_free_block();
    if (allocated_offset == -1) {
        // Free List 为空，从文件存储层追加新的数据区
        allocated_offset = stg_allocate_block(); 
        if (allocated_offset == -1) {
            Log("ERROR: Failed to allocate block from storage.");
            return -1;
        }
    }
    
    // 4. 将任务数据写入文件
    if (stg_write_task_block(allocated_offset, &new_task) != 0) {
        Log("ERROR: Failed to write task block to disk.");
        // 实际项目需要回滚 idx_allocate_free_block 或 stg_allocate_block
        return -1;
    }
    
    // 5. 更新内存索引
    if (idx_add_task_record(new_task.id, allocated_offset) != 0) {
        Log("ERROR: Failed to add index record.");
        return -1;
    }

    // 6. 递增下一个 ID
    idx_increment_next_id();

    // Log("INFO: Task %d added successfully at offset %ld.", new_task.id, allocated_offset);
    return new_id;
}

/**
 * @brief 根据ID查找任务记录。
 */
int db_find_task_by_id(int id, task_t *result_task) {
    if (id <= 0 || result_task == NULL) return -1;
    
    // 1. 通过内存索引查找文件偏移量
    long offset = idx_get_task_offset(id);
    if (offset == -1) {
        Log("INFO: Task ID %d not found in index.", id);
        return -1;
    }
    
    // 2. 从文件读取数据块
    if (stg_read_task_block(offset, result_task) != 0) {
        Log("ERROR: Failed to read task block at offset %ld.", offset);
        return -1;
    }
    
    return 0;
}

/**
 * @brief 更新现有任务的完整记录。
 * * 由于是定长记录，直接覆盖即可。
 */
int db_update_task(const task_t *updated_task) {
    if (updated_task == NULL || updated_task->id <= 0) return -1;
    
    // 1. 通过内存索引查找文件偏移量
    long offset = idx_get_task_offset(updated_task->id);
    if (offset == -1) {
        Log("ERROR: Cannot update, Task ID %d not found.", updated_task->id);
        return -1;
    }
    
    // 2. 将更新后的数据写入文件对应位置 (覆盖原数据)
    if (stg_write_task_block(offset, updated_task) != 0) {
        Log("ERROR: Failed to write updated task block at offset %ld.", offset);
        return -1;
    }

    // Log("INFO: Task %d updated successfully.", updated_task->id);
    return 0;
}

/**
 * @brief 删除任务并释放空间。
 */
int db_delete_task_by_id(int id) {
    if (id <= 0) return -1;
    
    // 1. 通过内存索引查找文件偏移量 (需要知道被删除块的位置)
    long offset = idx_get_task_offset(id);
    if (offset == -1) {
        Log("ERROR: Cannot delete, Task ID %d not found.", id);
        return -1;
    }

    // 2. 从内存索引中移除记录 (必须在释放空间之前，防止索引丢失)
    if (idx_remove_task_record(id) != 0) {
        Log("ERROR: Failed to remove index record for ID %d.", id);
        return -1;
    }
    
    // 3. 将该文件偏移量添加到空闲列表 (Free List)
    if (idx_free_block(offset) != 0) {
        Log("WARN: Failed to add block to free list. Space may not be reused.");
        // 如果 Free List 已满，我们仍然认为删除是成功的，但会浪费空间。
    }

    // Log("INFO: Task %d deleted and space at %ld freed.", id, offset);
    return 0;
}


// --- UTILITY FUNCTIONS ---

/**
 * @brief Gets the next available unique ID.
 */
int db_get_next_id(void) {
    // 调用 Index Manager 层来获取 Header 中的 next_id 字段
    return idx_get_next_id(); 
}

/**
 * @brief 获取总任务数。
 */
int db_get_task_count(void) {
    // 调用 Index Layer 提供的接口来获取计数
    return idx_get_task_count(); 
}

/**
 * @brief Prints the detailed information of a single task (for debugging/display).
 * * Modified to print all fields and format time_t to human-readable format.
 * @param task The task_t structure pointer to print.
 */
void db_print_task(const task_t *task) {
    if (task == NULL) {
        printf("[Task: NULL]\n");
        return;
    }

    // 用于存储格式化时间的缓冲区
    char created_time_str[TIME_STR_LEN];
    char due_time_str[TIME_STR_LEN];
    char completed_time_str[TIME_STR_LEN];
    
    // 格式化时间戳
    psr_readable_time(task->created_at, created_time_str, TIME_STR_LEN);
    psr_readable_time(task->due_date, due_time_str, TIME_STR_LEN);
    psr_readable_time(task->completed_at, completed_time_str, TIME_STR_LEN);

    printf("\n=== Task ===\n");
    printf("Task ID: %d\n", task->id);
    printf("Title: %s\n", task->title);

    if (strlen(task->description) > 0) {
        printf("Description: %s\n", task->description);
    } else {
        printf("Description: (None)\n");
    }

    printf("Status: %s\n", psr_status_string(task->stat));
    printf("Priority: %s\n", psr_priority_string(task->prio));

    // 打印格式化后的时间
    printf("Created At: %s\n", created_time_str);
    printf("Due Date: %s\n", due_time_str);
    printf("Completed At: %s\n", completed_time_str);
}

void db_print_all_task() {
    int task_count = 0;
    const index_record_t *index_p = idx_get_index(&task_count);

    task_t task; 
    long offset;
    
    if (index_p == NULL) return; 
    for (int i = 0; i < task_count; i++) {
        offset = index_p[i].offset;
        
        if (stg_read_task_block(offset, &task) != 0) {
            Log("ERROR: Failed to read task block for index %d.", i);
            continue; 
        }
        
        db_print_task(&task);
    }
}

void db_print_header() {
    const db_header_t *header_p = idx_get_header();
    stg_print_header(header_p);
}

char* db_get_all_tasks_json() {
    int task_count = 0;
    const index_record_t *index_p = idx_get_index(&task_count);
    
    if (task_count == 0 || index_p == NULL) {
        return strdup("[]"); 
    }

    // 1. 预估初始缓冲区大小
    // 头部 "[", 尾部 "]", 逗号分隔符, 加上所有任务的预估大小
    size_t initial_size = 2 + (task_count * ESTIMATED_TASK_JSON_SIZE); 
    char *result_buffer = (char*)malloc(initial_size);
    if (result_buffer == NULL) {
        Log("FATAL: Memory allocation failed for JSON array buffer.");
        return NULL;
    }

    // 初始化结果字符串为 JSON 数组的起始符号
    strcpy(result_buffer, "[");
    size_t current_len = 1;
    
    task_t task;
    char *task_json = NULL;

    // 2. 遍历所有索引记录
    for (int i = 0; i < task_count; i++) {
        long offset = index_p[i].offset; // 修正后的索引访问方式

        // 2a. 从文件读取任务数据
        if (stg_read_task_block(offset, &task) != 0) {
            Log("ERROR: Failed to read task block for index %d.", i);
            continue; // 跳过此任务
        }

        // 2b. 将 task_t 结构体序列化为单个 JSON 字符串
        task_json = psr_task_to_json(&task);
        if (task_json == NULL) {
            Log("ERROR: Failed to serialize task ID %d.", task.id);
            continue;
        }

        // 2c. 检查缓冲区是否需要扩展 (使用 realloc)
        size_t json_len = strlen(task_json);
        // 需要当前长度 + 新 JSON 长度 + 逗号/终止符
        if (current_len + json_len + 2 >= initial_size) {
            // 扩展缓冲区大小
            initial_size += (json_len + ESTIMATED_TASK_JSON_SIZE); 
            char *new_buffer = (char*)realloc(result_buffer, initial_size);
            if (new_buffer == NULL) {
                Log("FATAL: realloc failed during JSON array construction.");
                free(result_buffer);
                free(task_json);
                return NULL;
            }
            result_buffer = new_buffer;
        }

        // 2d. 追加逗号（如果不是第一个元素）
        if (i > 0) {
            strcat(result_buffer, ",");
            current_len += 1;
        }

        // 2e. 追加新的任务 JSON 字符串
        strcat(result_buffer, task_json);
        current_len += json_len;
        
        // 2f. 释放单个任务 JSON 字符串
        free(task_json);
        task_json = NULL;
    }

    // 3. 添加 JSON 数组的结束符号 "]"
    strcat(result_buffer, "]");
    
    // 4. 返回动态分配的 JSON 数组字符串
    return result_buffer;
}