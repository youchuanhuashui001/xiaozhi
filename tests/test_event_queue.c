#include <assert.h>
#include <string.h>

#include "event_queue.h"

int main(void)
{
	event_queue_t q;
	app_event_t ev = { .type = APP_EVENT_MANUAL_START };

	assert(event_queue_init(&q, 4) == 0);
	assert(event_queue_push(&q, &ev) == 0);

	memset(&ev, 0, sizeof(ev));
	assert(event_queue_pop(&q, &ev, 0) == 1);
	assert(ev.type == APP_EVENT_MANUAL_START);

	event_queue_destroy(&q);
	return 0;
}
