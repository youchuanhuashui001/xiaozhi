#include "event_queue.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int event_queue_wait_locked(event_queue_t *queue, int timeout_ms)
{
	if (timeout_ms <= 0)
		return queue->count > 0 ? 1 : 0;

	while (queue->count == 0) {
		struct timespec ts;
		int rc;

		clock_gettime(CLOCK_REALTIME, &ts);
		ts.tv_sec += timeout_ms / 1000;
		ts.tv_nsec += (timeout_ms % 1000) * 1000000L;
		if (ts.tv_nsec >= 1000000000L) {
			ts.tv_sec += 1;
			ts.tv_nsec -= 1000000000L;
		}

		rc = pthread_cond_timedwait(&queue->cond, &queue->mutex, &ts);
		if (rc == ETIMEDOUT)
			return 0;
		if (rc != 0)
			return -1;
	}

	return 1;
}

int event_queue_init(event_queue_t *queue, size_t capacity)
{
	if (!queue || capacity == 0)
		return -1;

	memset(queue, 0, sizeof(*queue));
	queue->items = calloc(capacity, sizeof(*queue->items));
	if (!queue->items)
		return -1;

	queue->capacity = capacity;
	pthread_mutex_init(&queue->mutex, NULL);
	pthread_cond_init(&queue->cond, NULL);

	return 0;
}

int event_queue_push(event_queue_t *queue, const app_event_t *event)
{
	if (!queue || !event)
		return -1;

	pthread_mutex_lock(&queue->mutex);

	if (queue->count == queue->capacity) {
		pthread_mutex_unlock(&queue->mutex);
		return -1;
	}

	queue->items[queue->tail] = *event;
	queue->tail = (queue->tail + 1) % queue->capacity;
	queue->count++;
	pthread_cond_signal(&queue->cond);
	pthread_mutex_unlock(&queue->mutex);

	return 0;
}

int event_queue_pop(event_queue_t *queue, app_event_t *event, int timeout_ms)
{
	int rc;

	if (!queue || !event)
		return -1;

	pthread_mutex_lock(&queue->mutex);
	rc = event_queue_wait_locked(queue, timeout_ms);
	if (rc != 1) {
		pthread_mutex_unlock(&queue->mutex);
		return rc;
	}

	*event = queue->items[queue->head];
	queue->head = (queue->head + 1) % queue->capacity;
	queue->count--;
	pthread_mutex_unlock(&queue->mutex);

	return 1;
}

void event_queue_destroy(event_queue_t *queue)
{
	if (!queue)
		return;

	free(queue->items);
	queue->items = NULL;
	queue->capacity = 0;
	queue->head = 0;
	queue->tail = 0;
	queue->count = 0;
	pthread_cond_destroy(&queue->cond);
	pthread_mutex_destroy(&queue->mutex);
}
