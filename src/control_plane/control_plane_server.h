#ifndef CONTROL_PLANE_SERVER_H
#define CONTROL_PLANE_SERVER_H

#include <libwebsockets.h>
#include <pthread.h>

#include "daemon/daemon_runtime.h"

typedef struct {
	daemon_runtime_t *runtime;
	int started;
	int stop_requested;
	struct lws_context *context;
	struct lws_protocols protocols[2];
	pthread_t thread;
	pthread_mutex_t event_lock;
	char last_outbound_json[1024];
	unsigned long outbound_seq;
	unsigned long last_runtime_seq;
} control_plane_server_t;

int control_plane_server_init(control_plane_server_t *server,
				 daemon_runtime_t *runtime);
int control_plane_server_start(control_plane_server_t *server);
void control_plane_server_stop(control_plane_server_t *server);
int control_plane_server_dispatch_json(control_plane_server_t *server,
					 const char *json);
const char *control_plane_server_last_outbound_json(
	const control_plane_server_t *server);

#endif /* CONTROL_PLANE_SERVER_H */
