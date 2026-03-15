#ifndef COMMAND_H
#define COMMAND_H

#include <stddef.h>

/**
 * 解析并执行客户端指令
 * @param json_str 客户端发送的 JSON 字符串
 * @param response_buf 用于存储响应结果的缓冲区
 * @param buf_size 缓冲区大小
 * @return 0 成功（包括逻辑错误但生成了响应），-1 严重解析失败
 */
int command_parse(const char *json_str, char *response_buf, size_t buf_size);

#endif /* COMMAND_H */
