#include "control_plane/control_plane_server.h"

#include <string.h>

int control_plane_server_init(control_plane_server_t *server,
				 daemon_runtime_t *runtime)
{
	if (!server)
		return -1;

	memset(server, 0, sizeof(*server));
	server->runtime = runtime;
	return 0;
}

int control_plane_server_start(control_plane_server_t *server)
{
	if (!server)
		return -1;

	server->started = 1;
	return 0;
}

void control_plane_server_stop(control_plane_server_t *server)
{
	if (!server)
		return;

	server->started = 0;
}

int control_plane_server_dispatch_json(control_plane_server_t *server,
					 const char *json)
{
	control_command_t cmd;

	if (!server || !server->runtime || !server->started || !json)
		return -1;

	memset(&cmd, 0, sizeof(cmd));
	if (control_protocol_parse_command(json, &cmd) != 0)
		return -1;

	return daemon_runtime_submit_command(server->runtime, &cmd);
}
