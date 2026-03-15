#ifndef MSG_QUEUE_H
#define MSG_QUEUE_H

#include <pthread.h>
#include <stddef.h>

#define MAX_MSG_LEN 256

typedef struct {
	char **buffer;
	int head;
	int tail;
	int count;
	int capacity;
	pthread_mutex_t mutex;
} msg_queue_t;

/**
 * 初始化消息队列
 * @param queue 队列指针
 * @param capacity 最大容量
 * @return 0 成功，-1 失败
 */
int msg_queue_init(msg_queue_t *queue, int capacity);

/**
 * 写入消息（如果队列满，则丢弃最旧的消息）
 * @param queue 队列指针
 * @param data 消息数据
 * @return 0 成功，-1 失败
 */
int msg_queue_push(msg_queue_t *queue, const char *data);

/**
 * 读取消息
 * @param queue 队列指针
 * @param buf 接收缓冲区
 * @param buf_size 缓冲区大小
 * @return 实际读取的长度，0 表示队列为空，-1 失败
 */
int msg_queue_pop(msg_queue_t *queue, char *buf, size_t buf_size);

/**
 * 销毁消息队列
 * @param queue 队列指针
 */
void msg_queue_destroy(msg_queue_t *queue);

#endif /* MSG_QUEUE_H */
