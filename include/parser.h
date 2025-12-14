#ifndef __PARSER_H__
#define __PARSER_H__

#include <stdio.h>
#include "database.h"

// --- PARSER API ---

/**
 * @brief 从 JSON 字符串解析任务数据，填充到 task_t 结构体中。
 * @param task_json 包含任务属性的 JSON 字符串。
 * @param task_out 指向接收任务数据的结构体指针。
 * @param require_id 标志：如果为 1，则要求 JSON 包含有效的 "id" 字段。
 * @return int 0 on success, -1 on failure (例如：JSON 格式错误或缺失必要字段)。
 */
int psr_json_to_task(const char *task_json, task_t *task_out, int require_id);

/**
 * @brief 将 task_t 结构体序列化为 JSON 字符串。
 * @param task_in 指向要序列化的 task_t 结构体指针。
 * @return char* 包含 JSON 数据的字符串指针。
 * 调用者必须负责使用 free() 释放此内存。
 */
char* psr_task_to_json(const task_t *task_in);

/**
 * @brief 将 time_t 时间戳转换为人类可读的字符串格式。
 * @param timestamp 要转换的时间戳。
 * @param buffer 接收格式化后字符串的缓冲区。
 * @param buffer_size 缓冲区大小。
 */
void psr_readable_time(time_t timestamp, char *buffer, size_t buffer_size);

/**
 * @brief 将任务状态枚举转换为字符串。
 */
const char* psr_status_string(task_status_e status);

/**
 * @brief 将任务优先级枚举转换为字符串。
 */
const char* psr_priority_string(task_priority_e priority);
#endif