#include <unistd.h>
#include "storage_manager.h"
#include "common.h"
#include "parser.h"

// --- 全局文件句柄定义 ---
// 在 storage_manager.c 中定义，并在 storage_manager.h 中 extern 声明
FILE *g_db_file = NULL;


// --- PRIVATE FUNCTION PROTOTYPES ---
static int _stg_init_db_file(db_header_t *header);


// --- STORAGE LIFECYCLE MANAGEMENT (stg_init, stg_shutdown) ---

static int _stg_init_db_file(db_header_t *header) {
    // 1. 初始化文件头默认值 (Metadata)
    memset(header, 0, DB_HEADER_SIZE);
    strncpy(header->magic, "TASK\0", 5);
    header->version = 1;
    header->next_id = 1; 
    header->index_count = 0;
    header->free_list_count = 0;

    // 2. 确定初始数据区末尾偏移量 (固定值)
    // 任务数据区从 DATA_START_OFFSET 开始。初始时，数据区末尾就是起始点。
    header->data_end_offset = DATA_START_OFFSET; 
    
    // --- A. 写入 Header ---
    if (stg_write_header(header) != 0) {
        Log("ERROR: stg_write_header failed.");
        return -1;
    }
    
    // --- B. 填充 Index Table 和 Free List 预分配区域 (定长) ---
    // 写入 INDEX_REGION_SIZE + FREE_LIST_REGION_SIZE 字节的零数据，用于初始化。
    
    long total_preallocated_size = INDEX_REGION_SIZE + FREE_LIST_REGION_SIZE;
    if (fseek(g_db_file, INDEX_OFFSET, SEEK_SET) != 0) {
        Log("ERROR: Seek to Index Offset failed.");
        return -1;
    }
    
    // 使用 DB_HEADER_SIZE (128) 作为零缓冲区大小进行循环写入
    char zero_buffer[DB_HEADER_SIZE]; 
    memset(zero_buffer, 0, DB_HEADER_SIZE);

    long bytes_written = 0;
    while (bytes_written < total_preallocated_size) {
        size_t write_size = (total_preallocated_size - bytes_written) > DB_HEADER_SIZE 
                            ? DB_HEADER_SIZE 
                            : (size_t)(total_preallocated_size - bytes_written);
        
        if (fwrite(zero_buffer, 1, write_size, g_db_file) != write_size) {
            Log("ERROR: Writing zero padding failed.");
            return -1;
        }
        bytes_written += write_size;
    }
    
    // 3. 截断文件，确保文件大小准确 (Header + 预分配区)
    // 文件末尾现在是 DATA_START_OFFSET
    if (ftruncate(fileno(g_db_file), header->data_end_offset) != 0) {
        Log("ERROR: Failed to truncate file on init.");
    }
    
    // 再次写入 Header，确保所有写入都已持久化
    if (stg_write_header(header) != 0) return -1;

    return 0;
}

/**
 * @brief 初始化存储层，打开数据库文件。
 */
int stg_init(void) {
    db_header_t header;
    long file_len = 0;

    // 1. 以读写更新模式打开文件 (rb+)
    // O_CREAT: 如果文件不存在，则创建它
    // "rb+": Read/Write, binary mode, start at beginning. Fails if file doesn't exist.
    // "r+b": Read/Write, binary mode, file must exist.
    // "w+b": Read/Write, binary mode, creates if doesn't exist, TRUNCATES if it does! (Avoid)
    
    // 最佳策略：尝试 r+b 打开，失败则 w+b 创建后立即关闭，再 r+b 打开。
    g_db_file = fopen(DB_FILENAME, "r+b"); 
    
    if (g_db_file == NULL) {
        // 文件不存在或权限问题，尝试创建
        g_db_file = fopen(DB_FILENAME, "w+b");
        if (g_db_file == NULL) {
            Log("ERROR: Can't create database file.");
            return -1;
        }

        // 文件已创建，但内容为空，需要初始化
        if (_stg_init_db_file(&header) != 0) {
            Log("ERROR: Initialize database failed");
            fclose(g_db_file);
            g_db_file = NULL;
            return -1;
        }
    } else {
        // 文件已存在，检查大小和 Header
        fseek(g_db_file, 0, SEEK_END);
        file_len = ftell(g_db_file);
        
        if (file_len < DB_HEADER_SIZE) {
            // 文件损坏或空，需要重新初始化
            Log("WARN: Database file corrupted, reinitializing...");
            if (_stg_init_db_file(&header) != 0) {
                // Log error
                fclose(g_db_file);
                g_db_file = NULL;
                return -1;
            }
        } else {
            // 读取 Header 进行校验
            if (stg_read_header(&header) != 0 || strncmp(header.magic, "TASK", 4) != 0) {
                Log("ERROR: Header verification failed. File type mismatch.");
                return -1;
            }
        }
    }

    // 确保文件指针回到开头
    fseek(g_db_file, 0, SEEK_SET);

    return 0;
}

/**
 * @brief 关闭数据库文件句柄并清理资源。
 */
void stg_shutdown(void) {
    if (g_db_file != NULL) {
        fclose(g_db_file);
        g_db_file = NULL;
    }
}


// --- I/O FUNCTIONS ---

/**
 * @brief 读取文件头。
 */
int stg_read_header(db_header_t *header) {
    if (g_db_file == NULL) return -1;
    if (fseek(g_db_file, 0, SEEK_SET) != 0) return -1;
    
    // 确保读取 DB_HEADER_SIZE 字节
    if (fread(header, DB_HEADER_SIZE, 1, g_db_file) != 1) return -1;
    
    return 0;
}

/**
 * @brief 写入文件头。
 */
int stg_write_header(const db_header_t *header) {
    if (g_db_file == NULL) return -1;
    if (fseek(g_db_file, 0, SEEK_SET) != 0) return -1;
    
    // 确保写入 DB_HEADER_SIZE 字节
    if (fwrite(header, DB_HEADER_SIZE, 1, g_db_file) != 1) {
        // 强制刷新缓冲区，确保数据写入磁盘
        fflush(g_db_file); 
        return -1;
    }
    fflush(g_db_file); // 写入成功，也强制刷新
    
    return 0;
}

/**
 * @brief 打印文件头。
 */
void stg_print_header(const db_header_t *header){
    if (g_db_file == NULL) {
        Log("ERROR: Database header not exists.");
        return;
    }
    if (fseek(g_db_file, 0, SEEK_SET) != 0) {
        Log("ERROR: Database header not exists.");
        return;
    }

    printf("=== Header ===\n");
    printf("magic: %s\n", header->magic);
    printf("version: %d\n", header->version);
    printf("next id: %d\n", header->next_id);
    printf("count: %d\n", header->index_count);
    printf("free list: %d\n", header->free_list_count);
    printf("data offset: %ld\n", header->data_end_offset);

}

/**
 * @brief 将内存中的索引表数组写入文件。
 */
int stg_write_index_table(long offset, const index_record_t *index_array, int count) {
    if (g_db_file == NULL) return -1;
    if (fseek(g_db_file, offset, SEEK_SET) != 0) return -1;
    
    size_t size_to_write = count * sizeof(index_record_t);
    
    if (fwrite(index_array, 1, size_to_write, g_db_file) != size_to_write) {
        fflush(g_db_file);
        return -1;
    }
    fflush(g_db_file);
    return 0;
}

/**
 * @brief 将内存中的空闲列表数组写入文件。
 */
int stg_write_free_list(long offset, const free_block_t *free_list_array, int count) {
    if (g_db_file == NULL) return -1;
    if (fseek(g_db_file, offset, SEEK_SET) != 0) return -1;
    
    size_t size_to_write = count * sizeof(free_block_t);
    
    if (fwrite(free_list_array, 1, size_to_write, g_db_file) != size_to_write) {
        fflush(g_db_file);
        return -1;
    }
    fflush(g_db_file);
    return 0;
}

/**
 * @brief 从指定偏移量读取单个任务数据块。
 */
int stg_read_task_block(long offset, task_t *task) {
    if (g_db_file == NULL) return -1;
    if (fseek(g_db_file, offset, SEEK_SET) != 0) return -1;
    
    // 确保读取 TASK_RECORD_SIZE 字节
    if (fread(task, TASK_RECORD_SIZE, 1, g_db_file) != 1) return -1;
    
    return 0;
}

/**
 * @brief 将单个任务数据块写入指定偏移量。
 */
int stg_write_task_block(long offset, const task_t *task) {
    if (g_db_file == NULL) return -1;
    if (fseek(g_db_file, offset, SEEK_SET) != 0) return -1;
    
    // 确保写入 TASK_RECORD_SIZE 字节
    if (fwrite(task, TASK_RECORD_SIZE, 1, g_db_file) != 1) {
        fflush(g_db_file);
        return -1;
    }
    fflush(g_db_file); // 写入成功后，强制刷新
    
    return 0;
}

/**
 * @brief 从空闲列表或文件末尾分配一个任务数据块的空间。
 * @return long 分配到的起始字节偏移量，-1 表示失败。
 */
long stg_allocate_block(void) {
    db_header_t header;
    long allocated_offset;

    if (stg_read_header(&header) != 0) {
        // Log error
        return -1;
    }

    if (header.free_list_count > 0) {
        // A. 从空闲列表分配空间 (LIFO 策略: 使用最后一个记录)

        // 1. 计算空闲列表末尾记录的偏移量 (使用固定的 FREE_LIST_OFFSET)
        size_t free_list_record_size = FREE_BLOCK_RECORD_SIZE; // 使用宏
        long free_list_end_offset = FREE_LIST_OFFSET 
                                  + (header.free_list_count - 1) * free_list_record_size;
        
        free_block_t free_block;
        // 2. 定位并读取最后一个空闲块记录
        if (fseek(g_db_file, free_list_end_offset, SEEK_SET) != 0 || 
            fread(&free_block, free_list_record_size, 1, g_db_file) != 1) 
        {
            Log("ERROR: Can't read free list.");
            return -1;
        }

        // 分配空间是空闲块的偏移量
        allocated_offset = free_block.offset;

        // 3. 更新 Header: 空闲块数量减少 1
        header.free_list_count--;
        
        // 4. 更新 Header 中 Free List 的相关信息 
        if (stg_write_header(&header) != 0) return -1;

        
    } else {
        // B. 从文件末尾追加空间 (Data Area)
        // 任务数据区从 DATA_START_OFFSET 开始增长
        allocated_offset = header.data_end_offset;
        
        // 更新 Header: 数据区末尾偏移量增加 TASK_RECORD_SIZE
        header.data_end_offset += TASK_RECORD_SIZE;
        
        if (stg_write_header(&header) != 0) return -1;
    }

    // 返回分配到的空间偏移量
    return allocated_offset;
}

/**
 * @brief 释放一个任务数据块的空间，将其添加到空闲列表。
 * @param offset 被释放块的起始字节偏移量。
 * @return int 0 on success, -1 on failure.
 */
int stg_free_block(long offset) {
    db_header_t header;
    free_block_t new_free_block;

    if (stg_read_header(&header) != 0) {
        // Log error
        return -1;
    }

    // 1. 检查 Free List 是否已满
    if (header.free_list_count >= MAX_TASKS) {
        Log("WARN: Free List is full, cannot reuse space.");
        // 在这种简化设计中，我们忽略这个块，牺牲空间，保持代码简单。
        return 0; 
    }

    // 2. 准备新的空闲块记录
    new_free_block.offset = offset;
    new_free_block.size = TASK_RECORD_SIZE;

    // 3. 计算空闲列表新的末尾偏移量 (使用固定的 FREE_LIST_OFFSET)
    size_t free_list_record_size = FREE_BLOCK_RECORD_SIZE; // 使用宏
    long new_free_list_offset = FREE_LIST_OFFSET 
                              + header.free_list_count * free_list_record_size;
    
    // 4. 将新空闲块记录追加到文件中的空闲列表末尾
    if (fseek(g_db_file, new_free_list_offset, SEEK_SET) != 0) return -1;
    if (fwrite(&new_free_block, free_list_record_size, 1, g_db_file) != 1) {
        fflush(g_db_file);
        Log("ERROR: Writing free block failed.");
        return -1;
    }
    fflush(g_db_file);

    // 5. 更新 Header: 空闲块数量增加 1
    header.free_list_count++;
    
    // 6. 写入更新后的 Header
    if (stg_write_header(&header) != 0) return -1;

    return 0;
}