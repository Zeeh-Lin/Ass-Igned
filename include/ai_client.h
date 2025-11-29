#ifndef __AI_CLIENT_H_
#define __AI_CLIENT_H__

#include <common.h>

// --- 1. 配置常量 (Constants) ---

// 替换成您真实的 API Key
#define AIC_API_KEY "***REMOVED***"
#define AIC_URL     "https://api.deepseek.com/chat/completions"
#define AIC_MODEL   "deepseek-chat"

// --- 2. 函数接口 (Public Interface) ---

/**
 * @brief 初始化AI客户端和所有必要的库。
 * 在使用任何其他AIC函数之前必须调用。
 * @return 0 成功，非0 失败。
 */
int aic_init(void);

/**
 * @brief 调用DeepSeek API发送一个请求。
 * * @param prompt 要发送给AI的用户问题。
 * @return char* AI的回复内容。返回NULL表示调用失败。
 * 注意: 返回的字符串是动态分配的，调用者必须使用 free() 释放。
 */
char* aic_call(const char *prompt);

/**
 * @brief 清理所有全局资源，在程序结束前调用。
 */
void aic_cleanup(void);

#endif // AI_CLIENT_H