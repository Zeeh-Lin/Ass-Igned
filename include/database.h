#ifndef __DATABASE_H__
#define __DATABASE_H__

#include <time.h>
#include <stdio.h>

// --- MACROS ---

// The maximum size for task title/description strings.
#define TASK_TITLE_MAX_LEN 128
#define TASK_DESC_MAX_LEN 256

// --- ENUMS ---

/**
 * @brief Defines the status of a task for scheduling and display.
 */
typedef enum {
    TASK_STATUS_TODO = 0,       // Task has been created, pending.
    TASK_STATUS_DOING = 1,      // Task is currently in progress.
    TASK_STATUS_DONE = 2,       // Task has been successfully completed.
    TASK_STATUS_DELETED = 3     // Task is soft-deleted (archived).
} task_status_e;

/**
 * @brief Defines the priority level of a task (for the Scheduler).
 */
typedef enum {
    PRIORITY_URGENT = 0,        // Highest priority, must be done now.
    PRIORITY_IMPORTANT = 1,
    PRIORITY_MEDIUM = 2,
    PRIORITY_LOW = 3            // Lowest priority.
} task_priority_e;

// --- CORE DATA STRUCTURE ---

/**
 * @brief The core data structure for a single task item.
 * * This structure is designed to be fixed-size for direct binary file I/O.
 */
typedef struct {
    int id;                     // Database primary key (unique ID).
    char title[TASK_TITLE_MAX_LEN];     // Task's brief title.
    char description[TASK_DESC_MAX_LEN];// Detailed task information.
    time_t created_at;          // Timestamp when the task was created.
    time_t due_date;            // The required completion time (deadline).
    time_t completed_at;        // Unix timestamp of completion (0 if pending).
    task_priority_e prio;       // Priority level.
    task_status_e stat;         // Current status of the task.
} task_t;

// Task Data Start

typedef struct {
    char magic[5];          // 文件魔数，例如 "TASK"
    int version;            // 数据库版本号 (例如 1)
    int next_id;            // 下一个可分配的唯一任务ID
    int index_count;        // 当前活动的任务数量（索引记录数量）
    int free_list_count;    // 空闲列表中记录的数量
    long data_end_offset;   // 实际任务数据区末尾的偏移量 (用于追加新任务)
    char padding[99];       // 填充到 DB_HEADER_SIZE
} db_header_t;

/**
 * @brief Free block record structure.
 * * Used to record reusable space left by deleted tasks.
 */
typedef struct {
    long offset;            // Starting byte offset of free block in file
    size_t size;            // Size of free block (TASK_RECORD_SIZE for fixed-length storage)
} free_block_t;


/**
 * @brief 内存中的索引记录结构体 (与文件中的 index_record_t 逻辑相同)
 * * 用于将任务ID映射到文件中的位置。
 */
typedef struct {
    int id;                 // 任务唯一ID
    long offset;            // 任务记录在文件中的起始偏移量
    size_t size;            // 任务记录大小 (TASK_RECORD_SIZE)
} index_record_t;

// --- DATABASE LIFECYCLE MANAGEMENT FUNCTIONS ---

/**
 * @brief Initializes the database by reading file headers and indices into memory.
 * * It does NOT load all task data. Returns 0 if DB file is created/loaded successfully.
 * @return int 0 on success, -1 on fatal error.
 */
int db_init(const char* db_file);

/**
 * @brief Saves the current memory state (indices and header) to the database file.
 * * Task data blocks are assumed to be written immediately on update/add.
 * @return int 0 on success, -1 on failure.
 */
int db_save_db(void);

/**
 * @brief Cleans up all memory allocated by the database module (indices, etc.).
 */
void db_shutdown(void);


// --- TASK OPERATION (CRUD) FUNCTIONS ---

/**
 * @brief Adds a new task to the database by parsing a JSON string.
 * * Writes the new task record to the file and updates the memory index.
 * @param task_json The raw JSON string containing task attributes (title, desc, due_date, prio).
 * @return int New task's ID (>= 1) on success, or -1 on failure (e.g., invalid JSON/IO error).
 */
int db_add_task(const char *task_json);

/**
 * @brief Finds a single task by its unique ID.
 * * Reads the record directly from the file into the result buffer.
 * @param id The ID of the task to find.
 * @param result_task Pointer to the task_t buffer where the found data will be copied.
 * @return int 0 on success, -1 if the task ID was not found or IO failed.
 */
int db_find_task_by_id(int id, task_t *result_task);

/**
 * @brief Updates an existing task's full record in the database file.
 * * Uses the ID in updated_task to find the offset and overwrite the block.
 * @param updated_task The task_t structure containing the new data (matched by ID).
 * @return int 0 on success, -1 if the task ID was not found or IO failed.
 */
int db_update_task(const task_t *updated_task);

/**
 * @brief Deletes a task by ID. The disk space is marked as free for future reuse.
 * * Updates the memory index and the file's Free List.
 * @param id The ID of the task to delete.
 * @return int 0 on success, -1 if the task ID was not found.
 */
int db_delete_task_by_id(int id);


// --- UTILITY FUNCTIONS ---

/**
 * @brief Gets the next available unique ID from the database header.
 * * This function is a wrapper for idx_get_next_id() in the Index Manager Layer.
 * @return int The next unique ID (>= 1).
 */
int db_get_next_id(void);

/**
 * @brief Gets the total number of active tasks currently in the database.
 * @return int The total active task count.
 */
int db_get_task_count(void);


void db_print_task(const task_t *task);
void db_print_all_task();
void db_print_header();
char* db_get_all_tasks_json(void);

#endif