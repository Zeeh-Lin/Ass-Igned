#ifndef __STORAGE_MANAGER_H__
#define __STORAGE_MANAGER_H__

#include <stdio.h>
#include <stdlib.h>
#include "database.h"
#include "index_manager.h"

// --- CORE FILE/BLOCK SIZES ---

#define DB_HEADER_SIZE 128
#define TASK_RECORD_SIZE sizeof(task_t)

#define MAX_TASKS 512
#define INDEX_RECORD_SIZE sizeof(index_record_t)
#define FREE_BLOCK_RECORD_SIZE sizeof(free_block_t)

#define INDEX_REGION_SIZE (MAX_TASKS * INDEX_RECORD_SIZE)
#define FREE_LIST_REGION_SIZE (MAX_TASKS * FREE_BLOCK_RECORD_SIZE)

#define INDEX_OFFSET DB_HEADER_SIZE // Index Start
#define FREE_LIST_OFFSET (INDEX_OFFSET + INDEX_REGION_SIZE) // Free List Start
#define DATA_START_OFFSET (FREE_LIST_OFFSET + FREE_LIST_REGION_SIZE) 

// --- FILE POINTER EXPOSURE ---

/**
 * @brief Database file handle.
 * * Managed internally by storage_manager.c, but exposed via extern to other storage_manager functions.
 * * Applications should not directly manipulate this pointer.
 */
extern FILE *g_db_file;


// --- STORAGE LIFECYCLE MANAGEMENT ---

/**
 * @brief Initialize storage layer, open database file.
 * * If file doesn't exist, create and initialize file header and structures.
 * @return int 0 on success, -1 on failure.
 */
int stg_init(void);

/**
 * @brief Close database file handle and clean up resources.
 */
void stg_shutdown(void);


// --- HEADER I/O FUNCTIONS ---

/**
 * @brief Read file header.
 * @param header Pointer to structure to receive data.
 * @return int 0 on success, -1 on failure.
 */
int stg_read_header(db_header_t *header);

/**
 * @brief Write file header.
 * @param header Pointer to structure to write.
 * @return int 0 on success, -1 on failure.
 */
int stg_write_header(const db_header_t *header);

void stg_print_header(const db_header_t *header) ;

// --- TASK BLOCK I/O FUNCTIONS ---

/**
 * @brief Read single task data block from specified offset.
 * @param offset Starting offset of task record in file.
 * @param task Pointer to task_t structure to receive data.
 * @return int 0 on success, -1 on failure.
 */
int stg_read_task_block(long offset, task_t *task);

/**
 * @brief Write single task data block to specified offset.
 * @param offset Starting offset of task record in file.
 * @param task Pointer to task_t structure to write.
 * @return int 0 on success, -1 on failure.
 */
int stg_write_task_block(long offset, const task_t *task);

long stg_allocate_block(void);

#endif