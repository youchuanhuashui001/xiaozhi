#include "msg_queue.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int msg_queue_init(msg_queue_t *queue, int capacity)
{
	queue->capacity = capacity;
	queue->head = 0;
	queue->tail = 0;
	queue->count = 0;
	queue->buffer = malloc(sizeof(char *) * capacity);
	if (!queue->buffer)
		return -1;

	for (int i = 0; i < capacity; i++) {
		queue->buffer[i] = malloc(MAX_MSG_LEN);
		if (!queue->buffer[i]) {
			// 清理已分配的内存
			for (int j = 0; j < i; j++)
				free(queue->buffer[j]);
			free(queue->buffer);
			return -1;
		}
	}

	if (pthread_mutex_init(&queue->mutex, NULL) != 0) {
		for (int i = 0; i < capacity; i++)
			free(queue->buffer[i]);
		free(queue->buffer);
		return -1;
	}

	return 0;
}

int msg_queue_push(msg_queue_t *queue, const char *data)
{
	pthread_mutex_lock(&queue->mutex);

	if (queue->count == queue->capacity) {
		// 队列满，丢弃头部的（最旧的），移动 head
		queue->head = (queue->head + 1) % queue->capacity;
		queue->count--;
		// 注意：这里我们选择总是覆盖，所以 count 减少后下面会再次增加
	}

	strncpy(queue->buffer[queue->tail], data, MAX_MSG_LEN - 1);
	queue->buffer[queue->tail][MAX_MSG_LEN - 1] = '\0';
	queue->tail = (queue->tail + 1) % queue->capacity;
	queue->count++;

	pthread_mutex_unlock(&queue->mutex);
	return 0;
}

int msg_queue_pop(msg_queue_t *queue, char *buf, size_t buf_size)
{
	int len = 0;
	pthread_mutex_lock(&queue->mutex);

	if (queue->count > 0) {
		strncpy(buf, queue->buffer[queue->head], buf_size - 1);
		buf[buf_size - 1] = '\0';
		len = strlen(buf);
		queue->head = (queue->head + 1) % queue->capacity;
		queue->count--;
	}

	pthread_mutex_unlock(&queue->mutex);
	return len;
}

void msg_queue_destroy(msg_queue_t *queue)
{
	pthread_mutex_destroy(&queue->mutex);
	for (int i = 0; i < queue->capacity; i++)
		free(queue->buffer[i]);
	free(queue->buffer);
}
