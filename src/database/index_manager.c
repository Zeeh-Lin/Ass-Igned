#include "index_manager.h"
#include "storage_manager.h"
#include "common.h"

static index_record_t g_index_table[MAX_TASKS]; 
static free_block_t g_free_list[MAX_TASKS];
static db_header_t g_db_header_cache;

// --- LIFECYCLE MANAGEMENT (idx_init, idx_shutdown) ---

/**
 * @brief 初始化索引管理器。
 * * 从数据库文件中读取 Header, Index Table 和 Free List 到内存。
 */
int idx_init(const char* db_file) {
    // 1. 启动底层存储（打开或创建文件）
    if (stg_init(db_file) != 0) {
        Log("ERROR: storage_manager initialization failed.");
        return -1;
    }

    // 2. 读取文件头到缓存
    if (stg_read_header(&g_db_header_cache) != 0) {
        Log("ERROR: Reading database header failed.");
        // 如果文件是新创建的（stg_init已处理），则理论上不会失败。
        stg_shutdown();
        return -1;
    }

    // 3. 读取 Index Table 到内存
    // 写入 INDEX_OFFSET 处，读取 g_db_header_cache.index_count * sizeof(index_record_t) 字节
    if (g_db_header_cache.index_count > 0) {
        size_t size_to_read = g_db_header_cache.index_count * INDEX_RECORD_SIZE;
        
        // 使用 stg_read_index_table 包装文件I/O，它需要读取 INDEX_OFFSET 处的索引数据
        // 由于 stg_read_index_table 未在 storage_manager.h 中声明，我们直接调用 fread
        if (fseek(g_db_file, INDEX_OFFSET, SEEK_SET) != 0 || 
            fread(g_index_table, 1, size_to_read, g_db_file) != size_to_read) 
        {
            Log("ERROR: Reading Index Table failed.");
            stg_shutdown();
            return -1;
        }
    }
    // 注意：g_index_table 数组中未被 count 覆盖的部分保持为 0 (初始时的 memset 或未使用的记录)


    // 4. 读取 Free List 到内存
    if (g_db_header_cache.free_list_count > 0) {
        size_t size_to_read = g_db_header_cache.free_list_count * FREE_BLOCK_RECORD_SIZE;
        
        // 同样直接调用 fread
        if (fseek(g_db_file, FREE_LIST_OFFSET, SEEK_SET) != 0 || 
            fread(g_free_list, 1, size_to_read, g_db_file) != size_to_read) 
        {
            Log("ERROR: Reading Free List failed.");
            stg_shutdown();
            return -1;
        }
    }

    return 0;
}


/**
 * @brief 关闭索引管理器，将内存中的 Index Table 和 Free List 写回文件。
 */
void idx_shutdown(void) {
    // 1. 将缓存的 Header 写回文件
    if (stg_write_header(&g_db_header_cache) != 0) {
        Log("ERROR: Failed to write final header during shutdown.");
        // 继续尝试写入索引和列表，但不返回
    }

    // 2. 将 Index Table 写回文件
    size_t index_size_to_write = g_db_header_cache.index_count * INDEX_RECORD_SIZE;
    
    if (fseek(g_db_file, INDEX_OFFSET, SEEK_SET) != 0 || 
        fwrite(g_index_table, 1, index_size_to_write, g_db_file) != index_size_to_write) 
    {
        Log("ERROR: Failed to write Index Table during shutdown.");
        // 继续尝试写入 Free List
    } else {
        fflush(g_db_file);
    }

    // 3. 将 Free List 写回文件
    size_t free_list_size_to_write = g_db_header_cache.free_list_count * FREE_BLOCK_RECORD_SIZE;

    if (fseek(g_db_file, FREE_LIST_OFFSET, SEEK_SET) != 0 || 
        fwrite(g_free_list, 1, free_list_size_to_write, g_db_file) != free_list_size_to_write) 
    {
        Log("ERROR: Failed to write Free List during shutdown.");
    } else {
        fflush(g_db_file);
    }
    
    // 4. 关闭底层存储
    stg_shutdown();
    Log("INFO: Index manager shut down and data persisted.");
}


// --- METADATA (Header) ACCESSORS ---

/**
 * @brief 获取当前活动任务的总数。
 */
int idx_get_task_count(void) {
    // 直接返回 Header 缓存中的索引计数
    return g_db_header_cache.index_count;
}

/**
 * @brief 获取下一个可用的任务ID。
 */
int idx_get_next_id(void) {
    return g_db_header_cache.next_id;
}

/**
 * @brief 增加下一个可用的任务ID。
 */
void idx_increment_next_id(void) {
    g_db_header_cache.next_id++;
}


// --- ID LOOKUP / INDEX OPERATIONS ---

/**
 * @brief 根据任务ID查找任务在文件中的偏移量。
 * * 使用线性搜索，简单但有效（对于 MAX_TASKS = 512 的规模）。
 */
long idx_get_task_offset(int id) {
    if (id <= 0) return -1;
    
    // 线性搜索 (适用于小规模 MAX_TASKS)
    for (int i = 0; i < g_db_header_cache.index_count; i++) {
        if (g_index_table[i].id == id) {
            return g_index_table[i].offset;
        }
    }
    
    return -1;
}

/**
 * @brief 添加一个新的任务索引记录到内存中。
 */
int idx_add_task_record(int id, long offset) {
    // 1. 检查索引表是否已满
    if (g_db_header_cache.index_count >= MAX_TASKS) {
        Log("ERROR: Index table is full.");
        return -1;
    }
    
    // 2. 检查 ID 是否冲突 (防止逻辑错误，虽然 next_id 保证唯一)
    if (idx_get_task_offset(id) != -1) {
        Log("ERROR: Attempted to add duplicate ID.");
        return -1;
    }

    // 3. 将记录追加到索引表的末尾 (内存操作)
    int new_index = g_db_header_cache.index_count;
    g_index_table[new_index].id = id;
    g_index_table[new_index].offset = offset;
    g_index_table[new_index].size = TASK_RECORD_SIZE; // 定长

    // 4. 更新 Header 计数
    g_db_header_cache.index_count++;
    
    return 0;
}

/**
 * @brief 从内存中移除任务索引记录。
 * * 使用“末尾替换”法，避免移动大量元素，效率高。
 */
int idx_remove_task_record(int id) {
    int removed_index = -1;
    
    // 1. 找到要移除记录的索引
    for (int i = 0; i < g_db_header_cache.index_count; i++) {
        if (g_index_table[i].id == id) {
            removed_index = i;
            break;
        }
    }
    
    if (removed_index == -1) {
        Log("ERROR: Cannot remove index, ID not found.");
        return -1;
    }

    // 2. 使用 LIFO (末尾元素) 填充被移除的空位
    int last_index = g_db_header_cache.index_count - 1;

    // 只有当被移除的不是最后一个元素时，才需要替换
    if (removed_index != last_index) {
        g_index_table[removed_index] = g_index_table[last_index];
    }
    
    // 3. 将最后一个元素的 ID 设为 0 (逻辑清除)
    g_index_table[last_index].id = 0;
    
    // 4. 更新 Header 计数
    g_db_header_cache.index_count--;
    
    return 0;
}

/**
 * @brief 获取内存中所有活动索引记录的列表。
 */
const index_record_t *idx_get_index(int *count_ptr) {
    if (count_ptr == NULL) {
        Log("ERROR: idx_get_all_index_records received NULL count_ptr.");
        return NULL;
    }
    
    // 返回活动索引的数量
    *count_ptr = g_db_header_cache.index_count;

    return g_index_table;
}

// --- FREE LIST MANAGEMENT ---

/**
 * @brief 尝试从 Free List 中分配一个可用块的偏移量。
 */
long idx_allocate_free_block(void) {
    if (g_db_header_cache.free_list_count == 0) {
        return -1; // 没有空闲块
    }

    // 1. LIFO 策略: 使用 Free List 数组的最后一个记录
    int last_index = g_db_header_cache.free_list_count - 1;
    long allocated_offset = g_free_list[last_index].offset;
    
    // 2. 逻辑移除：将最后一个记录的 offset 设为 0 (可选，但推荐)
    g_free_list[last_index].offset = 0; 

    // 3. 更新 Header 计数
    g_db_header_cache.free_list_count--;
    
    return allocated_offset;
}

/**
 * @brief 将一个被释放的块的偏移量添加到 Free List。
 */
int idx_free_block(long offset) {
    // 1. 检查 Free List 是否已满
    if (g_db_header_cache.free_list_count >= MAX_TASKS) {
        Log("WARN: Free List is full, discarding freed block.");
        return -1;
    }

    // 2. 将新记录追加到 Free List 数组的末尾
    int new_index = g_db_header_cache.free_list_count;
    g_free_list[new_index].offset = offset;
    g_free_list[new_index].size = TASK_RECORD_SIZE;

    // 3. 更新 Header 计数
    g_db_header_cache.free_list_count++;
    
    return 0;
}

/**
 * @brief 获取内存中空闲块记录的列表。
 */
const free_block_t *idx_get_free_list(int *count_ptr) {
    if (count_ptr == NULL) {
        Log("ERROR: idx_get_all_free_blocks received NULL count_ptr.");
        return NULL;
    }
    
    // 返回空闲列表中的数量
    *count_ptr = g_db_header_cache.free_list_count;

    return g_free_list;
}

const db_header_t *idx_get_header() {
    return &g_db_header_cache;
}