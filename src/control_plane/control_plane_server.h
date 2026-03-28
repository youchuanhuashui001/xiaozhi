#ifndef CONTROL_PLANE_SERVER_H
#define CONTROL_PLANE_SERVER_H

#include "daemon/daemon_runtime.h"

typedef struct {
	daemon_runtime_t *runtime;
	int started;
	char last_outbound_json[1024];
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
