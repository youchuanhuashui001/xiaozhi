#ifndef EVENT_QUEUE_H
#define EVENT_QUEUE_H

#include <pthread.h>
#include <stddef.h>

typedef enum {
	APP_EVENT_NONE = 0,
	APP_EVENT_MANUAL_START,
	APP_EVENT_MANUAL_STOP,
	APP_EVENT_WAKEWORD_DETECTED,
	APP_EVENT_WS_CONNECTED,
	APP_EVENT_WS_HELLO,
	APP_EVENT_WS_TEXT,
	APP_EVENT_WS_BINARY,
	APP_EVENT_CAPTURE_SILENCE_TIMEOUT,
	APP_EVENT_PLAYBACK_FINISHED,
	APP_EVENT_ERROR,
	APP_EVENT_SHUTDOWN
} app_event_type_t;

typedef struct {
	app_event_type_t type;
	int code;
	size_t data_len;
	char text[1024];
} app_event_t;

typedef struct {
	app_event_t *items;
	size_t capacity;
	size_t head;
	size_t tail;
	size_t count;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
} event_queue_t;

int event_queue_init(event_queue_t *queue, size_t capacity);
int event_queue_push(event_queue_t *queue, const app_event_t *event);
int event_queue_pop(event_queue_t *queue, app_event_t *event, int timeout_ms);
void event_queue_destroy(event_queue_t *queue);

#endif /* EVENT_QUEUE_H */
