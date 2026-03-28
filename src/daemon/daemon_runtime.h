#ifndef DAEMON_RUNTIME_H
#define DAEMON_RUNTIME_H

#include <pthread.h>

#include "app.h"
#include "control_plane/control_protocol.h"

typedef enum {
	DAEMON_RUNTIME_COMMAND_NONE = 0,
	DAEMON_RUNTIME_COMMAND_CONNECT_SERVER,
	DAEMON_RUNTIME_COMMAND_DISCONNECT_SERVER,
	DAEMON_RUNTIME_COMMAND_SHUTDOWN_CLIENT,
	DAEMON_RUNTIME_COMMAND_UNKNOWN
} daemon_runtime_command_t;

typedef struct {
	app_runtime_t *app;
	daemon_runtime_command_t last_command;
	control_command_t last_control_command;
	control_event_t last_control_event;
	unsigned long observed_event_count;
	pthread_mutex_t lock;
} daemon_runtime_t;

int daemon_runtime_init(daemon_runtime_t *rt, app_runtime_t *app);
int daemon_runtime_submit_command(daemon_runtime_t *rt,
				     const control_command_t *cmd);
int daemon_runtime_snapshot(daemon_runtime_t *rt, control_event_t *event,
			    unsigned long *observed_event_count);
void daemon_runtime_destroy(daemon_runtime_t *rt);

#endif /* DAEMON_RUNTIME_H */
