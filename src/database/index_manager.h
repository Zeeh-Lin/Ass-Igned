// index_manager.h

#ifndef __INDEX_MANAGER_H__
#define __INDEX_MANAGER_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "database.h"

int idx_init(const char* db_file);
void idx_shutdown(void);

long idx_get_task_offset(int id);
int idx_get_task_count(void);
int idx_get_next_id(void);
void idx_increment_next_id(void);
int idx_add_task_record(int id, long offset);
int idx_remove_task_record(int id);

long idx_allocate_free_block(void);
int idx_free_block(long offset);

const index_record_t *idx_get_index(int *count_ptr);
const free_block_t *idx_get_free_list(int *count_ptr);
const db_header_t *idx_get_header();

#endif