#ifndef CONTROL_PLANE_SERVER_H
#define CONTROL_PLANE_SERVER_H

#include "daemon/daemon_runtime.h"

typedef struct {
	daemon_runtime_t *runtime;
	int started;
} control_plane_server_t;

int control_plane_server_init(control_plane_server_t *server,
				 daemon_runtime_t *runtime);
int control_plane_server_start(control_plane_server_t *server);
void control_plane_server_stop(control_plane_server_t *server);
int control_plane_server_dispatch_json(control_plane_server_t *server,
					 const char *json);

#endif /* CONTROL_PLANE_SERVER_H */
